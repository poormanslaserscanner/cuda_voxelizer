[vt,tris] = readPLY('\win_pmls\bin\test.ply');
tris = tris';
trpnts = (single(vt(tris(:),:)))';
bbox = [(min(trpnts'))', (max(trpnts'))'];
[vol,t] = cudavoxmex(trpnts,bbox,100);
