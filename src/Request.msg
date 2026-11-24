message Request
{
    double sizeBits;
    simtime_t deadline;
}

message TransferFile extends Request
{
    double remainingBits;
    double chunkBits;
    double credits;
    bool delivered;
    simtime_t readyAt;
}

message SelectionQuery
{
    double sizeBits;
    simtime_t deadline;
    int currentAccess = -1;
    int selectedAccess = -1;
}

message LinkCondition
{
    double factor;
}
