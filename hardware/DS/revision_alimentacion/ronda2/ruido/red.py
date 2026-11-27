#!/usr/bin/env python3
"""red.py <red> [...] — lista cada nodo de la red con value, footprint y LCSC (de componentes.tsv)."""
import sys, os, re
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
comp = {}
for l in open(os.path.join(D, 'componentes.tsv')):
    f = l.rstrip('\n').split('\t')
    if len(f) >= 4 and not l.startswith('#'):
        comp[f[0]] = f[1:]
redes = {}
for l in open(os.path.join(D, 'redes.txt')):
    if l.startswith('#'): continue
    f = l.rstrip('\n').split('\t')
    redes[f[0]] = f[2].split()
for n in sys.argv[1:]:
    print(f'== {n} ({len(redes.get(n, []))} nodos)')
    for nodo in redes.get(n, []):
        ref = nodo.split('.')[0]
        c = comp.get(ref, ['?'])
        print(f'   {nodo:28s} {c[0]:14s} {c[1][:38] if len(c)>1 else "":38s} {c[2] if len(c)>2 else ""}')
