import pcbnew
board = pcbnew.GetBoard()
nm = board.GetNetInfo()
outpath = '/Users/bergamifer/Library/CloudStorage/Dropbox/Proceso Domo/Electronica - Domosonica/DS/librerias/netlist.md'
lines = []
for netcode in range(nm.GetNetCount()):
    net = nm.GetNetItem(netcode)
    if net is None:
        continue
    name = net.GetNetname()
    pads = [f"{p.GetParentAsString()}/{p.GetNumber()}" for p in board.GetPads() if p.GetNetname() == name]
    if pads:
        line = f"{name}: {', '.join(pads)}"
        lines.append(line)
        print(line)
with open(outpath, 'w') as f:
    f.write('\n'.join(lines) + '\n')
print(f"\nNetlist saved to {outpath} ({len(lines)} nets)")


# import pcbnew
# board = pcbnew.GetBoard()
# for fp in board.GetFootprints():
#     ref = fp.Reference()
#     ref.SetTextSize(pcbnew.VECTOR2I(pcbnew.FromMM(0.8), pcbnew.FromMM(0.8)))
#     ref.SetTextThickness(pcbnew.FromMM(0.12))
# pcbnew.Refresh()
# board.Save(board.GetFileName())


#exec(open('/Users/bergamifer/Library/CloudStorage/Dropbox/Proceso Domo/Electronica - Domosonica/DS/librerias/netlist.py').read())
