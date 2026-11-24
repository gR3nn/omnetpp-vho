simple FileServer
{
    parameters:
        double handoverDelay @unit(s) = default(0.3s);
        double scanInterval @unit(s) = default(0.1s);
        @display("i=block/queue");
    gates:
        input in;
        input ack;
        inout selection;
        output networkOut[2];
        output expiredOut;
}

simple HandoverAlgorithm
{
    parameters:
        string policy = default("fastest");
        double valuePerMB = default(0.9);
        double idealTime @unit(s);
        double deadlineValueFraction;
        double surplusMargin = default(0.01);
        @display("i=block/control");
    gates:
        inout server;
}

simple FileDestination
{
    parameters:
        double valuePerMB = default(0.9);
        double idealTime @unit(s);
        double deadlineValueFraction;
        @display("i=block/sink");
    gates:
        input in[];
        output ack;
}

simple DataBuffer
{
    parameters:
        @display("i=block/queue");
    gates:
        input in[];
        output out;
}

simple ChunkDelay
{
    parameters:
        double peakRate @unit(Mbps);
        double price;
        double chunkSize @unit(B) = default(1500B);
        double pollInterval @unit(s);
        @display("i=block/delay");
    gates:
        input in;
        input loadIn;
        input radioIn;
        output out;
}

simple TransferLoop
{
    parameters:
        @display("i=block/routing");
    gates:
        input in;
        output again;
        output done;
}

simple LoadConditions
{
    parameters:
        double interval @unit(s);
        double variation = default(0.25);
        @display("i=block/source");
    gates:
        output out;
}

simple RadioConditions
{
    parameters:
        double interval @unit(s);
        double center @unit(m);
        double radius @unit(m);
        double routeLength @unit(m);
        double initialPosition @unit(m);
        double speed @unit(mps);
        @display("i=block/source");
    gates:
        output out;
}

module AccessNetwork
{
    parameters:
        double center @unit(m) = default(300m);
        double radius @unit(m);
        double peakRate @unit(Mbps);
        double price;
        double routeLength @unit(m);
        double initialPosition @unit(m);
        double speed @unit(mps);
        double interval @unit(s) = default(0.1s);
        @display("i=abstract/cloud");
    gates:
        input in;
        output out;
    submodules:
        databuffer: DataBuffer { @display("p=100,180"); }
        genLoadCond: LoadConditions {
            interval = parent.interval;
            @display("p=160,55");
        }
        genRadioCond: RadioConditions {
            interval = parent.interval;
            center = parent.center;
            radius = parent.radius;
            routeLength = parent.routeLength;
            initialPosition = parent.initialPosition;
            speed = parent.speed;
            @display("p=360,55");
        }
        dly: ChunkDelay {
            peakRate = parent.peakRate;
            price = parent.price;
            pollInterval = parent.interval;
            @display("p=255,115");
        }
        loop: TransferLoop { @display("p=380,180"); }
    connections:
        in --> databuffer.in++;
        databuffer.out --> dly.in;
        genLoadCond.out --> dly.loadIn;
        genRadioCond.out --> dly.radioIn;
        dly.out --> loop.in;
        loop.again --> databuffer.in++;
        loop.done --> out;
}

network ModularVerticalHandover
{
    parameters:
        double speed @unit(mps) = default(12mps);
        double initialPosition @unit(m) = default(0m);
        double routeLength @unit(m) = default(600m);
        double valuePerMB = default(0.9);
        double idealTime @unit(s) = default(2s);
        double deadlineValueFraction = default(0.25);
        @display("bgb=650,365");
    submodules:
        gen: Traffic {
            modular = true;
            @display("p=85,155");
        }
        svr: FileServer { @display("p=230,155"); }
        alg: HandoverAlgorithm {
            valuePerMB = parent.valuePerMB;
            idealTime = parent.idealTime;
            deadlineValueFraction = parent.deadlineValueFraction;
            @display("p=230,280");
        }
        network1: AccessNetwork {
            radius = 310m;
            peakRate = 3Mbps;
            price = default(0.18);
            routeLength = parent.routeLength;
            initialPosition = parent.initialPosition;
            speed = parent.speed;
            @display("p=405,75;t=Cellular");
        }
        network2: AccessNetwork {
            radius = 145m;
            peakRate = 12Mbps;
            price = default(0.04);
            routeLength = parent.routeLength;
            initialPosition = parent.initialPosition;
            speed = parent.speed;
            @display("p=405,240;t=Wi-Fi");
        }
        dest: FileDestination {
            valuePerMB = parent.valuePerMB;
            idealTime = parent.idealTime;
            deadlineValueFraction = parent.deadlineValueFraction;
            @display("p=565,155");
        }
    connections:
        gen.out --> svr.in;
        svr.selection <--> alg.server;
        svr.networkOut[0] --> network1.in;
        svr.networkOut[1] --> network2.in;
        network1.out --> dest.in++;
        network2.out --> dest.in++;
        svr.expiredOut --> dest.in++;
        dest.ack --> svr.ack;
}
