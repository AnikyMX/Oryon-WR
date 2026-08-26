#!/usr/bin/env python3
"""
extract_mc_callsites.py — sumber kebenaran CALL SITE.

Memindai constant pool seluruh kelas di 1.12.2.jar dan mengumpulkan setiap
Methodref/Fieldref yang menunjuk ke org/lwjgl/*. Hasilnya adalah permukaan API
LWJGL yang benar-benar disentuh Minecraft, bukan tebakan.

Keluaran: data/mc_callsites.json
"""
import json, os, sys, zipfile, collections

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jclass

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JAR = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("MC_JAR")
TAG = sys.argv[2] if len(sys.argv) > 2 else "1.12.2"
OUT = os.path.join(ROOT, "data", "mc_callsites.json")

z = zipfile.ZipFile(JAR)
methods = collections.Counter()   # "Owner.name" -> jumlah kelas perujuk
fields = collections.Counter()
owners = collections.Counter()
descs = {}
users = collections.defaultdict(set)
n_cls = n_bad = 0

for e in z.namelist():
    if not e.endswith(".class"):
        continue
    try:
        cf = jclass.parse(z.read(e))
    except Exception:
        n_bad += 1
        continue
    n_cls += 1
    for tag, owner, name, desc in cf.refs:
        if not owner.startswith("org/lwjgl/"):
            continue
        short = owner.rsplit("/", 1)[-1]
        key = short + "." + name
        owners[owner] += 1
        if tag == 9:
            fields[key] += 1
        else:
            methods[key] += 1
            descs.setdefault(key, set()).add(desc)
        users[key].add(e)
z.close()

data = {
    "source": os.path.basename(JAR),
    "version": TAG,
    "classes_scanned": n_cls,
    "classes_unparsed": n_bad,
    "owners": dict(owners.most_common()),
    "method_refs": dict(methods.most_common()),
    "field_refs": dict(fields.most_common()),
    "method_descs": {k: sorted(v) for k, v in descs.items()},
}
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w") as f:
    json.dump(data, f, indent=1)

print("kelas dipindai      :", n_cls, "| gagal:", n_bad)
print("paket LWJGL disentuh:", len(owners))
print("method unik         :", len(methods))
print("field unik          :", len(fields))
print("-> " + OUT)
print()
print("--- pemilik terbanyak ---")
for k, v in owners.most_common(20):
    print("  %5d  %s" % (v, k))
