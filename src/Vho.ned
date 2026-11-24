simple Access
{
    parameters:
        string technology;
        double center @unit(m);
        double radius @unit(m);
        double peakRate @unit(Mbps);
        double price; // credits per decimal MB
        double variation = default(0.25);
        @display("i=device/antennatower");
}

simple Traffic
{
    parameters:
        bool modular = default(false);
        int files = default(20);
        double interval @unit(s) = default(5s);
        double fileSize @unit(MB) = default(4MB);
        double deadline @unit(s) = default(20s);
        @display("i=block/source");
    gates:
        output out;
}

simple Terminal
{
    parameters:
        double tick @unit(s) = default(0.1s);
        double speed @unit(mps) = default(12mps);
        double routeLength @unit(m) = default(600m);
        double initialPosition @unit(m) = default(0m);
        string policy = default("fastest");
        double handoverDelay @unit(s) = default(0.3s);
        double valuePerMB = default(0.9);
        double surplusMargin = default(0.01);
        @display("i=device/laptop");
    gates:
        input in;
}

network VerticalHandover
{
    submodules:
        cellular: Access {
            technology = "Cellular";
            center = 300m;
            radius = 310m;
            peakRate = 3Mbps;
            price = default(0.18);
            @display("p=300,80");
        }
        wifi: Access {
            technology = "Wi-Fi";
            center = 300m;
            radius = default(145m);
            peakRate = 12Mbps;
            price = default(0.04);
            @display("p=460,80");
        }
        traffic: Traffic {
            @display("p=140,240");
        }
        terminal: Terminal {
            @display("p=300,240");
        }
    connections:
        traffic.out --> terminal.in;
}
