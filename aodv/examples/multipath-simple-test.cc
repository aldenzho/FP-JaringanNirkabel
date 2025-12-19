#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultipathSimpleTest");

int main(int argc, char *argv[])
{
    LogComponentEnable("AodvRoutingTable", LOG_LEVEL_DEBUG);
    
    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);
    
    // Simple mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(50.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    
    // Install AODV
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);
    
    // Setup simple network
    NetDeviceContainer devices;
    devices = NodeContainer::CreateDeviceContainer(nodes);
    
    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);
    
    // Test multipath table directly
    Ptr<Node> node = nodes.Get(0);
    Ptr<aodv::RoutingProtocol> aodvProtocol = node->GetObject<aodv::RoutingProtocol>();
    
    if (aodvProtocol) {
        Ptr<aodv::RoutingTable> rt = aodvProtocol->GetRoutingTable();
        
        // Test add multipath routes
        rt->AddMultipathRoute(Ipv4Address("10.1.1.3"), Ipv4Address("10.1.1.2"), 2, Seconds(10));
        rt->AddMultipathRoute(Ipv4Address("10.1.1.3"), Ipv4Address("10.1.1.4"), 1, Seconds(10));
        
        // Print routing table
        Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
        rt->Print(routingStream);
    }
    
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();
    
    return 0;
}