using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Threading;

// A loopback-only fault injector. It never accepts or forwards remote traffic.
public static class StarfallUdpRelay {
    sealed class Packet { public byte[] Data; public int Side; public long Due; }
    public static void Run(int port, int players, string directory, bool impairStartup) {
        int links = players * (players - 1);
        var sockets = new UdpClient[links];
        var sourcePeer = new int[links];
        var targetPeer = new int[links];
        Func<int,int,int> link = (from,to) => from*(players-1) + (to<from ? to : to-1);
        for(int from=0;from<players;from++) for(int to=0;to<players;to++) if(from!=to) {
            int id=link(from,to);
            sourcePeer[id]=from;targetPeer[id]=to;
            sockets[id]=new UdpClient(new IPEndPoint(IPAddress.Loopback,port+players+id));
        }
        // The first peer starts before the second binds. Ignore Windows' UDP
        // connection-reset notification for those initial ICMP port-unreachable
        // replies; later datagrams must continue to use the same sockets.
        foreach(var socket in sockets)
            socket.Client.IOControl((IOControlCode)unchecked((int)0x9800000C), new byte[4], null);
        var clock = Stopwatch.StartNew();
        var pending = new List<Packet>();
        long received=0, forwarded=0, dropped=0, blackout=0, blackoutStart=-1, lastReport=-1;
        var directions = new long[links];
        bool impair=impairStartup;
        Action report = () => File.WriteAllText(Path.Combine(directory,"relay.json"),
            "{\"received\":"+received+",\"forwarded\":"+forwarded+",\"dropped\":"+dropped+
            ",\"blackoutDropped\":"+blackout + ",\"directions\":["+string.Join(",",directions)+"]"+
            ",\"delayMs\":50,\"jitterMs\":21,\"dropEvery\":23,\"blackoutMs\":2000}");
        File.WriteAllText(Path.Combine(directory,"relay.ready"),"127.0.0.1");
        try {
            while(clock.ElapsedMilliseconds < 240000 && !File.Exists(Path.Combine(directory,"relay.stop"))) {
                long now=clock.ElapsedMilliseconds;
                if(!impair && File.Exists(Path.Combine(directory,"peer0","samples","20.txt"))) impair=true;
                if(!impairStartup && blackoutStart<0 && File.Exists(Path.Combine(directory,"peer0","samples","240.txt"))) blackoutStart=now;
                for(int side=0;side<links;side++) {
                    while(sockets[side].Available>0) {
                        IPEndPoint source = new IPEndPoint(IPAddress.Any,0);
                        byte[] bytes=sockets[side].Receive(ref source);
                        if(!source.Address.Equals(IPAddress.Loopback) || source.Port!=port+sourcePeer[side]) continue;
                        if(impairStartup && received==0) blackoutStart=now+8000;
                        received++;
                        if(blackoutStart>=0 && now>=blackoutStart && now-blackoutStart<2000) {blackout++;continue;}
                        if(impair && received%23==0) {dropped++;continue;}
                        pending.Add(new Packet {Data=bytes,Side=side,Due=impair ? now+50+(received%4)*7 : now});
                    }
                }
                for(int i=0;i<pending.Count;) {
                    if(pending[i].Due>now) {i++;continue;}
                    var packet=pending[i];
                    int other=targetPeer[packet.Side];
                    sockets[link(other,sourcePeer[packet.Side])].Send(packet.Data,packet.Data.Length,new IPEndPoint(IPAddress.Loopback,port+other));
                    forwarded++;directions[packet.Side]++;pending.RemoveAt(i);
                }
                if(now-lastReport>500) {report();lastReport=now;}
                Thread.Sleep(1);
            }
        } finally {report();foreach(var socket in sockets) socket.Dispose();}
    }
}
