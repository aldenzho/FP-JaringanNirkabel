#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-rtable.h"

using namespace ns3;
using namespace ns3::aodv;

NS_LOG_COMPONENT_DEFINE("MultipathSimpleTest");

int main(int argc, char *argv[])
{
    LogComponentEnable("AodvRoutingTable", LOG_LEVEL_DEBUG);
    LogComponentEnable("MultipathSimpleTest", LOG_LEVEL_INFO);
    
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
    
    // Setup WiFi devices
    WifiHelper wifi;
    WifiMacHelper wifiMac;
    YansWifiChannelHelper wifiChannel;
    YansWifiPhyHelper wifiPhy;
    
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", 
                                   "MaxRange", DoubleValue(100.0));
    
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);
    
    // Install AODV
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);
    
    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);
    
    std::cout << "Node 0 IP: " << interfaces.GetAddress(0) << std::endl;
    std::cout << "Node 1 IP: " << interfaces.GetAddress(1) << std::endl;
    std::cout << "Node 2 IP: " << interfaces.GetAddress(2) << std::endl;
    
    // Test 1: Test MultipathRouteEntry secara langsung
    std::cout << "\n=== TEST 1: MultipathRouteEntry Direct Test ===" << std::endl;
    MultipathRouteEntry multipathEntry(Ipv4Address("10.1.1.3"));
    
    // Add paths
    multipathEntry.AddPath(Ipv4Address("10.1.1.2"), 2, Seconds(10));
    multipathEntry.AddPath(Ipv4Address("10.1.1.4"), 3, Seconds(10));
    multipathEntry.AddPath(Ipv4Address("10.1.1.5"), 1, Seconds(10));
    
    // Get best path
    auto bestPath = multipathEntry.GetBestPath();
    std::cout << "Best path: via " << bestPath.nextHop << " with hops: " << bestPath.hopCount << std::endl;
    
    // Get all paths
    auto allPaths = multipathEntry.GetAllPaths();
    std::cout << "Total paths: " << allPaths.size() << std::endl;
    for (const auto& path : allPaths) {
        std::cout << "  - Via " << path.nextHop << " hops: " << path.hopCount << std::endl;
    }
    
    // Test 2: Test RoutingTable multipath melalui AODV protocol
    std::cout << "\n=== TEST 2: RoutingTable Multipath Test ===" << std::endl;
    
    Ptr<Node> node = nodes.Get(0);
    Ptr<aodv::RoutingProtocol> aodvProtocol = node->GetObject<aodv::RoutingProtocol>();
    
    if (aodvProtocol) {
        // PERBAIKAN: Gunakan Reference bukan Pointer
        RoutingTable& rt = aodvProtocol->GetRoutingTable();
        
        // Test add multipath routes (gunakan . bukan ->)
        rt.AddMultipathRoute(Ipv4Address("10.1.1.10"), Ipv4Address("10.1.1.2"), 2, Seconds(10));
        rt.AddMultipathRoute(Ipv4Address("10.1.1.10"), Ipv4Address("10.1.1.3"), 1, Seconds(10));
        rt.AddMultipathRoute(Ipv4Address("10.1.1.10"), Ipv4Address("10.1.1.4"), 3, Seconds(10));
        
        // Get best multipath route
        MultipathRouteEntry::PathInfo best;
        if (rt.GetBestMultipathRoute(Ipv4Address("10.1.1.10"), best)) {
            std::cout << "Found best multipath route to 10.1.1.10 via: " << best.nextHop 
                      << " with hops: " << best.hopCount << std::endl;
        }
        
        // Get all multipath routes
        auto allMultipathRoutes = rt.GetAllMultipathRoutes(Ipv4Address("10.1.1.10"));
        std::cout << "Total multipath routes to 10.1.1.10: " << allMultipathRoutes.size() << std::endl;
        for (const auto& route : allMultipathRoutes) {
            std::cout << "  - Via " << route.nextHop << " hops: " << route.hopCount << std::endl;
        }
        
        // Print routing table
        std::cout << "\n=== Routing Table ===" << std::endl;
        Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
        rt.Print(routingStream);
    }
    
    // Test 3: Create standalone RoutingTable untuk test
    std::cout << "\n=== TEST 3: Standalone RoutingTable Test ===" << std::endl;
    // Untuk standalone, kita tetap butuh CreateObject atau constructor
    // Tapi karena kita hapus SimpleRefCount, kita tidak bisa menggunakan Create<RoutingTable>
    // Jadi kita buat objek langsung
    RoutingTable standaloneRt(Seconds(10));
    
    standaloneRt.AddMultipathRoute(Ipv4Address("192.168.1.1"), Ipv4Address("192.168.1.2"), 2, Seconds(15));
    standaloneRt.AddMultipathRoute(Ipv4Address("192.168.1.1"), Ipv4Address("192.168.1.3"), 1, Seconds(15));
    
    MultipathRouteEntry::PathInfo standaloneBest;
    if (standaloneRt.GetBestMultipathRoute(Ipv4Address("192.168.1.1"), standaloneBest)) {
        std::cout << "Standalone test - Best route to 192.168.1.1 via: " << standaloneBest.nextHop << std::endl;
    }
    
    std::cout << "\n=== Simulation Completed ===" << std::endl;
    
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();
    
    return 0;
}
