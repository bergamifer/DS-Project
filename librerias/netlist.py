board = pcbnew.GetBoard()
nm = board.GetNetInfo()
for netcode in range(nm.GetNetCount()):
    net = nm.GetNetItem(netcode)
    name = net.GetNetname()
    pads = [f"{p.GetParentAsString()}/{p.GetNumber()}" for p in board.GetPads() if p.GetNetname() == name]
    if pads:
        print(f"{name}: {', '.join(pads)}")


# import pcbnew
# board = pcbnew.GetBoard()
# for fp in board.GetFootprints():
#     ref = fp.Reference()
#     ref.SetTextSize(pcbnew.VECTOR2I(pcbnew.FromMM(0.8), pcbnew.FromMM(0.8)))
#     ref.SetTextThickness(pcbnew.FromMM(0.12))
# pcbnew.Refresh()
# board.Save(board.GetFileName())