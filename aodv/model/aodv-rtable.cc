/*
 * Copyright (c) 2009 IITP RAS
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Based on
 *      NS-2 AODV model developed by the CMU/MONARCH group and optimized and
 *      tuned by Samir Das and Mahesh Marina, University of Cincinnati;
 *
 *      AODV-UU implementation by Erik Nordström of Uppsala University
 *      https://web.archive.org/web/20100527072022/http://core.it.uu.se/core/index.php/AODV-UU
 *
 * Authors: Elena Buchatskaia <borovkovaes@iitp.ru>
 *          Pavel Boyko <boyko@iitp.ru>
 */


#include "aodv-rtable.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <iomanip>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AodvRoutingTable");

namespace aodv
{

/*
 The Routing Table
 */

RoutingTableEntry::RoutingTableEntry(Ptr<NetDevice> dev,
                                     Ipv4Address dst,
                                     bool vSeqNo,
                                     uint32_t seqNo,
                                     Ipv4InterfaceAddress iface,
                                     uint16_t hops,
                                     Ipv4Address nextHop,
                                     Time lifetime)
    : m_ackTimer(Timer::CANCEL_ON_DESTROY),
      m_validSeqNo(vSeqNo),
      m_seqNo(seqNo),
      m_hops(hops),
      m_lifeTime(lifetime + Simulator::Now()),
      m_iface(iface),
      m_flag(VALID),
      m_reqCount(0),
      m_blackListState(false),
      m_blackListTimeout(Simulator::Now())
{
    m_ipv4Route = Create<Ipv4Route>();
    m_ipv4Route->SetDestination(dst);
    m_ipv4Route->SetGateway(nextHop);
    m_ipv4Route->SetSource(m_iface.GetLocal());
    m_ipv4Route->SetOutputDevice(dev);
}

RoutingTableEntry::~RoutingTableEntry()
{
}

bool
RoutingTableEntry::InsertPrecursor(Ipv4Address id)
{
    NS_LOG_FUNCTION(this << id);
    if (!LookupPrecursor(id))
    {
        m_precursorList.push_back(id);
        return true;
    }
    else
    {
        return false;
    }
}

bool
RoutingTableEntry::LookupPrecursor(Ipv4Address id)
{
    NS_LOG_FUNCTION(this << id);
    for (auto i = m_precursorList.begin(); i != m_precursorList.end(); ++i)
    {
        if (*i == id)
        {
            NS_LOG_LOGIC("Precursor " << id << " found");
            return true;
        }
    }
    NS_LOG_LOGIC("Precursor " << id << " not found");
    return false;
}

bool
RoutingTableEntry::DeletePrecursor(Ipv4Address id)
{
    NS_LOG_FUNCTION(this << id);
    auto i = std::remove(m_precursorList.begin(), m_precursorList.end(), id);
    if (i == m_precursorList.end())
    {
        NS_LOG_LOGIC("Precursor " << id << " not found");
        return false;
    }
    else
    {
        NS_LOG_LOGIC("Precursor " << id << " found");
        m_precursorList.erase(i, m_precursorList.end());
    }
    return true;
}

void
RoutingTableEntry::DeleteAllPrecursors()
{
    NS_LOG_FUNCTION(this);
    m_precursorList.clear();
}

bool
RoutingTableEntry::IsPrecursorListEmpty() const
{
    return m_precursorList.empty();
}

void
RoutingTableEntry::GetPrecursors(std::vector<Ipv4Address>& prec) const
{
    NS_LOG_FUNCTION(this);
    if (IsPrecursorListEmpty())
    {
        return;
    }
    for (auto i = m_precursorList.begin(); i != m_precursorList.end(); ++i)
    {
        bool result = true;
        for (auto j = prec.begin(); j != prec.end(); ++j)
        {
            if (*j == *i)
            {
                result = false;
                break;
            }
        }
        if (result)
        {
            prec.push_back(*i);
        }
    }
}

void
RoutingTableEntry::Invalidate(Time badLinkLifetime)
{
    NS_LOG_FUNCTION(this << badLinkLifetime.As(Time::S));
    if (m_flag == INVALID)
    {
        return;
    }
    m_flag = INVALID;
    m_reqCount = 0;
    m_lifeTime = badLinkLifetime + Simulator::Now();
}

void
RoutingTableEntry::Print(Ptr<OutputStreamWrapper> stream, Time::Unit unit /* = Time::S */) const
{
    std::ostream* os = stream->GetStream();
    // Copy the current ostream state
    std::ios oldState(nullptr);
    oldState.copyfmt(*os);

    *os << std::resetiosflags(std::ios::adjustfield) << std::setiosflags(std::ios::left);

    std::ostringstream dest;
    std::ostringstream gw;
    std::ostringstream iface;
    std::ostringstream expire;
    dest << m_ipv4Route->GetDestination();
    gw << m_ipv4Route->GetGateway();
    iface << m_iface.GetLocal();
    expire << std::setprecision(2) << (m_lifeTime - Simulator::Now()).As(unit);
    *os << std::setw(16) << dest.str();
    *os << std::setw(16) << gw.str();
    *os << std::setw(16) << iface.str();
    *os << std::setw(16);
    switch (m_flag)
    {
    case VALID: {
        *os << "UP";
        break;
    }
    case INVALID: {
        *os << "DOWN";
        break;
    }
    case IN_SEARCH: {
        *os << "IN_SEARCH";
        break;
    }
    }

    *os << std::setw(16) << expire.str();
    *os << m_hops << std::endl;
    // Restore the previous ostream state
    (*os).copyfmt(oldState);
}

/*
 The Routing Table
 */

RoutingTable::RoutingTable(Time t)
    : m_badLinkLifetime(t)
{
}

bool
RoutingTable::LookupRoute(Ipv4Address id, RoutingTableEntry& rt)
{
    NS_LOG_FUNCTION(this << id);
    Purge();
    if (m_ipv4AddressEntry.empty())
    {
        NS_LOG_LOGIC("Route to " << id << " not found; m_ipv4AddressEntry is empty");
        return false;
    }
    auto i = m_ipv4AddressEntry.find(id);
    if (i == m_ipv4AddressEntry.end())
    {
        NS_LOG_LOGIC("Route to " << id << " not found");
        return false;
    }
    rt = i->second;
    NS_LOG_LOGIC("Route to " << id << " found");
    return true;
}

bool
RoutingTable::LookupValidRoute(Ipv4Address id, RoutingTableEntry& rt)
{
    NS_LOG_FUNCTION(this << id);
    if (!LookupRoute(id, rt))
    {
        NS_LOG_LOGIC("Route to " << id << " not found");
        return false;
    }
    NS_LOG_LOGIC("Route to " << id << " flag is "
                             << ((rt.GetFlag() == VALID) ? "valid" : "not valid"));
    return (rt.GetFlag() == VALID);
}

bool
RoutingTable::DeleteRoute(Ipv4Address dst)
{
    NS_LOG_FUNCTION(this << dst);
    Purge();
    if (m_ipv4AddressEntry.erase(dst) != 0)
    {
        NS_LOG_LOGIC("Route deletion to " << dst << " successful");
        return true;
    }
    NS_LOG_LOGIC("Route deletion to " << dst << " not successful");
    return false;
}

bool
RoutingTable::AddRoute(RoutingTableEntry& rt)
{
    NS_LOG_FUNCTION(this);
    Purge();
    if (rt.GetFlag() != IN_SEARCH)
    {
        rt.SetRreqCnt(0);
    }
    auto result = m_ipv4AddressEntry.insert(std::make_pair(rt.GetDestination(), rt));
    return result.second;
}

bool
RoutingTable::Update(RoutingTableEntry& rt)
{
    NS_LOG_FUNCTION(this);
    auto i = m_ipv4AddressEntry.find(rt.GetDestination());
    if (i == m_ipv4AddressEntry.end())
    {
        NS_LOG_LOGIC("Route update to " << rt.GetDestination() << " fails; not found");
        return false;
    }
    i->second = rt;
    if (i->second.GetFlag() != IN_SEARCH)
    {
        NS_LOG_LOGIC("Route update to " << rt.GetDestination() << " set RreqCnt to 0");
        i->second.SetRreqCnt(0);
    }
    return true;
}

bool
RoutingTable::SetEntryState(Ipv4Address id, RouteFlags state)
{
    NS_LOG_FUNCTION(this);
    auto i = m_ipv4AddressEntry.find(id);
    if (i == m_ipv4AddressEntry.end())
    {
        NS_LOG_LOGIC("Route set entry state to " << id << " fails; not found");
        return false;
    }
    i->second.SetFlag(state);
    i->second.SetRreqCnt(0);
    NS_LOG_LOGIC("Route set entry state to " << id << ": new state is " << state);
    return true;
}

void
RoutingTable::GetListOfDestinationWithNextHop(Ipv4Address nextHop,
                                              std::map<Ipv4Address, uint32_t>& unreachable)
{
    NS_LOG_FUNCTION(this);
    Purge();
    unreachable.clear();
    for (auto i = m_ipv4AddressEntry.begin(); i != m_ipv4AddressEntry.end(); ++i)
    {
        if (i->second.GetNextHop() == nextHop)
        {
            NS_LOG_LOGIC("Unreachable insert " << i->first << " " << i->second.GetSeqNo());
            unreachable.insert(std::make_pair(i->first, i->second.GetSeqNo()));
        }
    }
}

void
RoutingTable::InvalidateRoutesWithDst(const std::map<Ipv4Address, uint32_t>& unreachable)
{
    NS_LOG_FUNCTION(this);
    Purge();
    for (auto i = m_ipv4AddressEntry.begin(); i != m_ipv4AddressEntry.end(); ++i)
    {
        for (auto j = unreachable.begin(); j != unreachable.end(); ++j)
        {
            if ((i->first == j->first) && (i->second.GetFlag() == VALID))
            {
                NS_LOG_LOGIC("Invalidate route with destination address " << i->first);
                i->second.Invalidate(m_badLinkLifetime);
            }
        }
    }
}

void
RoutingTable::DeleteAllRoutesFromInterface(Ipv4InterfaceAddress iface)
{
    NS_LOG_FUNCTION(this);
    if (m_ipv4AddressEntry.empty())
    {
        return;
    }
    for (auto i = m_ipv4AddressEntry.begin(); i != m_ipv4AddressEntry.end();)
    {
        if (i->second.GetInterface() == iface)
        {
            auto tmp = i;
            ++i;
            m_ipv4AddressEntry.erase(tmp);
        }
        else
        {
            ++i;
        }
    }
}

void
RoutingTable::Purge()
{
    NS_LOG_FUNCTION(this);
    if (m_ipv4AddressEntry.empty())
    {
        return;
    }
    for (auto i = m_ipv4AddressEntry.begin(); i != m_ipv4AddressEntry.end();)
    {
        if (i->second.GetLifeTime().IsStrictlyNegative())
        {
            if (i->second.GetFlag() == INVALID)
            {
                auto tmp = i;
                ++i;
                m_ipv4AddressEntry.erase(tmp);
            }
            else if (i->second.GetFlag() == VALID)
            {
                NS_LOG_LOGIC("Invalidate route with destination address " << i->first);
                i->second.Invalidate(m_badLinkLifetime);
                ++i;
            }
            else
            {
                ++i;
            }
        }
        else
        {
            ++i;
        }
    }
    // =========== PENAMBAHAN MULTIPATH ==============
    PurgeMultipathRoutes();
}

void
RoutingTable::Purge(std::map<Ipv4Address, RoutingTableEntry>& table) const
{
    NS_LOG_FUNCTION(this);
    if (table.empty())
    {
        return;
    }
    for (auto i = table.begin(); i != table.end();)
    {
        if (i->second.GetLifeTime().IsStrictlyNegative())
        {
            if (i->second.GetFlag() == INVALID)
            {
                auto tmp = i;
                ++i;
                table.erase(tmp);
            }
            else if (i->second.GetFlag() == VALID)
            {
                NS_LOG_LOGIC("Invalidate route with destination address " << i->first);
                i->second.Invalidate(m_badLinkLifetime);
                ++i;
            }
            else
            {
                ++i;
            }
        }
        else
        {
            ++i;
        }
    }
}

bool
RoutingTable::MarkLinkAsUnidirectional(Ipv4Address neighbor, Time blacklistTimeout)
{
    NS_LOG_FUNCTION(this << neighbor << blacklistTimeout.As(Time::S));
    auto i = m_ipv4AddressEntry.find(neighbor);
    if (i == m_ipv4AddressEntry.end())
    {
        NS_LOG_LOGIC("Mark link unidirectional to  " << neighbor << " fails; not found");
        return false;
    }
    i->second.SetUnidirectional(true);
    i->second.SetBlacklistTimeout(blacklistTimeout);
    i->second.SetRreqCnt(0);
    NS_LOG_LOGIC("Set link to " << neighbor << " to unidirectional");
    return true;
}

void
RoutingTable::Print(Ptr<OutputStreamWrapper> stream, Time::Unit unit /* = Time::S */) const
{
    std::map<Ipv4Address, RoutingTableEntry> table = m_ipv4AddressEntry;
    Purge(table);
    std::ostream* os = stream->GetStream();
    // Copy the current ostream state
    std::ios oldState(nullptr);
    oldState.copyfmt(*os);

    *os << std::resetiosflags(std::ios::adjustfield) << std::setiosflags(std::ios::left);
    *os << "\nAODV Routing table\n";
    *os << std::setw(16) << "Destination";
    *os << std::setw(16) << "Gateway";
    *os << std::setw(16) << "Interface";
    *os << std::setw(16) << "Flag";
    *os << std::setw(16) << "Expire";
    *os << "Hops" << std::endl;
    for (auto i = table.begin(); i != table.end(); ++i)
    {
        i->second.Print(stream, unit);
    }
    *stream->GetStream() << "\n";
}
// ==================== PENAMBAHAN MULTIPATH ====================

MultipathRouteEntry::MultipathRouteEntry()
  : m_destination(Ipv4Address())
{
}

MultipathRouteEntry::MultipathRouteEntry(Ipv4Address destination)
  : m_destination(destination)
{
}

void 
MultipathRouteEntry::AddPath(Ipv4Address nextHop, uint32_t hopCount, Time lifetime)
{
  NS_LOG_FUNCTION(this << nextHop << hopCount << lifetime.As(Time::S));
  
  // First, remove any expired paths
  auto now = Simulator::Now();
  m_paths.erase(
    std::remove_if(m_paths.begin(), m_paths.end(),
      [now](const PathInfo& path) {
        return path.expiryTime <= now;
      }),
    m_paths.end()
  );
  
  // Check if path already exists
  for (auto& path : m_paths) {
    if (path.nextHop == nextHop) {
      // Update existing path
      path.hopCount = hopCount;
      path.expiryTime = Simulator::Now() + lifetime;
      path.isValid = true;
      NS_LOG_LOGIC("Updated existing path to " << m_destination << " via " << nextHop);
      return;
    }
  }
  
  // Add new path
  PathInfo newPath;
  newPath.nextHop = nextHop;
  newPath.hopCount = hopCount;
  newPath.expiryTime = Simulator::Now() + lifetime;
  newPath.pathQuality = 1.0; // Default quality
  newPath.isValid = true;
  
  m_paths.push_back(newPath);
  NS_LOG_LOGIC("Added new path to " << m_destination << " via " << nextHop 
               << " with hop count " << hopCount << " and lifetime " << lifetime.As(Time::S));
}

void 
MultipathRouteEntry::RemovePath(Ipv4Address nextHop)
{
  NS_LOG_FUNCTION(this << nextHop);
  
  size_t initialSize = m_paths.size();
  m_paths.erase(
    std::remove_if(m_paths.begin(), m_paths.end(),
      [nextHop](const PathInfo& path) { 
        return path.nextHop == nextHop; 
      }),
    m_paths.end()
  );
  
  if (m_paths.size() < initialSize) {
    NS_LOG_LOGIC("Removed path to " << m_destination << " via " << nextHop);
  }
}
// ============== PENAMBAHAN MULTIPATH ==============
MultipathRouteEntry::PathInfo 
MultipathRouteEntry::GetBestPath()
{
  NS_LOG_FUNCTION(this);
  
  // Remove expired paths first
  auto now = Simulator::Now();
  m_paths.erase(
    std::remove_if(m_paths.begin(), m_paths.end(),
      [now](const PathInfo& path) {
        return path.expiryTime <= now;
      }),
    m_paths.end()
  );
  
  if (m_paths.empty()) {
    NS_LOG_LOGIC("No valid paths available for " << m_destination);
    return PathInfo(); // Return invalid path
  }
  
  // ============ PENAMBAHAN BLE-MAODV: Use multi-metric selection ================= 
  NS_LOG_LOGIC("Using BLE-MAODV multi-metric path selection for " << m_destination);

  WeightFactors weights;

  // Calculate scores for all paths
    for (auto& path : m_paths) {
        path.compositeScore = path.CalculateCompositeScore(weights);
        NS_LOG_DEBUG("Path via " << path.nextHop << 
                     " - Hops: " << path.hopCount <<
                     ", Energy: " << path.bleMetrics.residualEnergy <<
                     ", RSSI: " << path.bleMetrics.rssiValue <<
                     ", Stability: " << path.bleMetrics.stabilityScore <<
                     ", Score: " << path.compositeScore);
    }
  // ==================== BLE-MAODV END ===================

  // For now, we'll use hop count as primary metric
  auto bestPath = std::min_element(m_paths.begin(), m_paths.end(),
    [](const PathInfo& a, const PathInfo& b) {
      return a.hopCount < b.hopCount;
    });
  
  NS_LOG_LOGIC("Selected best path to " << m_destination << " via " << bestPath->nextHop 
               << " with hop count " << bestPath->hopCount);
  return *bestPath;
}

// ==================== END ==============

std::vector<MultipathRouteEntry::PathInfo> 
MultipathRouteEntry::GetAllPaths()
{
  NS_LOG_FUNCTION(this);
  
  // Remove expired paths first
  auto now = Simulator::Now();
  m_paths.erase(
    std::remove_if(m_paths.begin(), m_paths.end(),
      [now](const PathInfo& path) {
        return path.expiryTime <= now;
      }),
    m_paths.end()
  );
  
  NS_LOG_LOGIC("Returning " << m_paths.size() << " paths for " << m_destination);
  return m_paths;
}

bool 
MultipathRouteEntry::HasValidPath()
{
  // Remove expired paths
  auto now = Simulator::Now();
  m_paths.erase(
    std::remove_if(m_paths.begin(), m_paths.end(),
      [now](const PathInfo& path) {
        return path.expiryTime <= now;
      }),
    m_paths.end()
  );
  
  bool hasValid = !m_paths.empty();
  NS_LOG_LOGIC("Has valid path for " << m_destination << ": " << hasValid);
  return hasValid;
}

// ==================== PENAMBAHAN MULTIPATH ====================


// ==================== PENAMBAHAN BLE-MAODV IMPLEMENTATION ====================


// BLEMetrics constructor
BLEMetrics::BLEMetrics() 
    : residualEnergy(1.0), 
      rssiValue(-50.0), 
      stabilityScore(1.0), 
      hopCount(1),
      lastUpdated(Simulator::Now())
{
}

// PathInfo constructor  
MultipathRouteEntry::PathInfo::PathInfo() 
    : nextHop(Ipv4Address()), 
      hopCount(0), 
      expiryTime(Time()), 
      pathQuality(0.0), 
      isValid(false),
      compositeScore(0.0),
      lastUsed(Simulator::Now()),
      usageCount(0) 
{      
}

// CalculateCompositeScore implementation
double 
MultipathRouteEntry::PathInfo::CalculateCompositeScore(const WeightFactors& weights) const
{
    // Normalize hop count (lower is better)
    double hopScore = 1.0 / (1.0 + hopCount);
    
    // Use residual energy directly (higher is better)
    double energyScore = bleMetrics.residualEnergy;
    
    // Normalize RSSI (-100 dBm to -30 dBm range)
    double rssiScore = (bleMetrics.rssiValue + 100.0) / 70.0;
    rssiScore = std::max(0.0, std::min(1.0, rssiScore));
    
    // Use stability score directly
    double stabilityScore = bleMetrics.stabilityScore;
    
    // Calculate weighted composite score
    double score = (weights.hopWeight * hopScore) +
                   (weights.energyWeight * energyScore) +
                   (weights.rssiWeight * rssiScore) +
                   (weights.stabilityWeight * stabilityScore);
    
    return score;
}

// UpdateStabilityScore implementation
void 
MultipathRouteEntry::PathInfo::UpdateStabilityScore(bool successfulTransmission)
{
    double stabilityFactor = 0.1; // Learning rate
    double reward = successfulTransmission ? 1.0 : 0.0;
    
    // Update stability score using exponential moving average
    bleMetrics.stabilityScore = (1.0 - stabilityFactor) * bleMetrics.stabilityScore + 
                               stabilityFactor * reward;
    
    // Update usage statistics
    usageCount++;
    lastUsed = Simulator::Now();
}

// AdaptiveWeightCalculator implementation
AdaptiveWeightCalculator::AdaptiveWeightCalculator()
{
    // Initialize weight profiles for different scenarios
    
    // High density: prioritize energy efficiency
    m_highDensityWeights.hopWeight = 0.2;
    m_highDensityWeights.energyWeight = 0.5;
    m_highDensityWeights.rssiWeight = 0.2;
    m_highDensityWeights.stabilityWeight = 0.1;
    
    // High mobility: prioritize stability
    m_highMobilityWeights.hopWeight = 0.2;
    m_highMobilityWeights.energyWeight = 0.1;
    m_highMobilityWeights.rssiWeight = 0.2;
    m_highMobilityWeights.stabilityWeight = 0.5;
    
    // Energy critical: prioritize energy
    m_energyCriticalWeights.hopWeight = 0.1;
    m_energyCriticalWeights.energyWeight = 0.7;
    m_energyCriticalWeights.rssiWeight = 0.1;
    m_energyCriticalWeights.stabilityWeight = 0.1;
    
    // Default balanced weights
    m_defaultWeights.hopWeight = 0.4;
    m_defaultWeights.energyWeight = 0.2;
    m_defaultWeights.rssiWeight = 0.2;
    m_defaultWeights.stabilityWeight = 0.2;
}

WeightFactors
AdaptiveWeightCalculator::CalculateWeights(const NetworkContext& context) const
{
    WeightFactors weights;
    
    if (context.energyCriticality > HIGH_ENERGY_THRESHOLD) {
        weights = m_energyCriticalWeights;
    }
    else if (context.mobilityLevel > HIGH_MOBILITY_THRESHOLD) {
        weights = m_highMobilityWeights;
    }
    else if (context.nodeDensity > HIGH_DENSITY_THRESHOLD) {
        weights = m_highDensityWeights;
    }
    else {
        weights = m_defaultWeights;
    }
    
    // Apply traffic criticality adjustment
    if (context.trafficCriticality > 0.7) {
        // For real-time traffic, increase stability and hop weight
        weights.stabilityWeight += 0.1;
        weights.hopWeight += 0.1;
        weights.energyWeight = std::max(0.1, weights.energyWeight - 0.1);
        weights.rssiWeight = std::max(0.1, weights.rssiWeight - 0.1);
    }
    
    weights.Normalize();
    
    return weights;
}

void
AdaptiveWeightCalculator::UpdateNetworkContext(NetworkContext& context, 
                                              uint32_t neighborCount,
                                              double avgEnergy,
                                              double mobilityIndicator) const
{
    // Update node density (normalize based on expected max neighbors)
    context.nodeDensity = std::min(1.0, neighborCount / 20.0);
    
    // Update energy criticality (lower average energy = higher criticality)
    context.energyCriticality = 1.0 - avgEnergy;
    
    // Update mobility level
    context.mobilityLevel = mobilityIndicator;
}

// ==================== END BLE-MAODV IMPLEMENTATION ====================

// ==================== MULTIPATH ROUTING TABLE METHODS ====================

bool
RoutingTable::AddMultipathRoute(Ipv4Address dst, Ipv4Address nextHop, uint32_t hopCount, Time lifetime)
{
    NS_LOG_FUNCTION(this << dst << nextHop << hopCount << lifetime);
    
    // Cari entri untuk destination
    auto it = m_multipathTable.find(dst);
    if (it == m_multipathTable.end()) {
        // Buat entri baru jika belum ada
        MultipathRouteEntry newEntry(dst);
        newEntry.AddPath(nextHop, hopCount, lifetime);
        m_multipathTable[dst] = newEntry;
        NS_LOG_DEBUG("Created new multipath entry for " << dst << " with path via " << nextHop);
    } else {
        // Tambahkan path ke entri yang sudah ada
        it->second.AddPath(nextHop, hopCount, lifetime);
        NS_LOG_DEBUG("Added path to existing multipath entry for " << dst << " via " << nextHop);
    }
    
    return true;
}

bool
RoutingTable::GetBestMultipathRoute(Ipv4Address dst, MultipathRouteEntry::PathInfo& pathInfo)
{
    NS_LOG_FUNCTION(this << dst);
    
    auto it = m_multipathTable.find(dst);
    if (it != m_multipathTable.end()) {
        pathInfo = it->second.GetBestPath();
        if (pathInfo.isValid) {
            NS_LOG_DEBUG("Found best multipath route to " << dst << " via " << pathInfo.nextHop);
            return true;
        }
    }
    
    NS_LOG_DEBUG("No valid multipath route found for " << dst);
    return false;
}

std::vector<MultipathRouteEntry::PathInfo>
RoutingTable::GetAllMultipathRoutes(Ipv4Address dst)
{
    NS_LOG_FUNCTION(this << dst);
    
    auto it = m_multipathTable.find(dst);
    if (it != m_multipathTable.end()) {
        return it->second.GetAllPaths();
    }
    
    return std::vector<MultipathRouteEntry::PathInfo>();
}

bool
RoutingTable::HasMultipathRoute(Ipv4Address dst)
{
    NS_LOG_FUNCTION(this << dst);
    
    auto it = m_multipathTable.find(dst);
    if (it != m_multipathTable.end()) {
        return it->second.HasValidPath();
    }
    
    return false;
}

bool
RoutingTable::RemoveMultipathRoute(Ipv4Address dst, Ipv4Address nextHop)
{
    NS_LOG_FUNCTION(this << dst << nextHop);
    
    auto it = m_multipathTable.find(dst);
    if (it != m_multipathTable.end()) {
        it->second.RemovePath(nextHop);
        NS_LOG_DEBUG("Removed path via " << nextHop << " from multipath entry for " << dst);
        
        // Jika tidak ada path lagi, hapus entri
        if (!it->second.HasValidPath()) {
            m_multipathTable.erase(it);
            NS_LOG_DEBUG("Removed empty multipath entry for " << dst);
        }
        return true;
    }
    
    NS_LOG_DEBUG("No multipath entry found for " << dst << " to remove path via " << nextHop);
    return false;
}

void
RoutingTable::PurgeMultipathRoutes()
{
    NS_LOG_FUNCTION(this);
    
    for (auto it = m_multipathTable.begin(); it != m_multipathTable.end(); ) {
        // Hapus path yang sudah kadaluarsa
        it->second.GetBestPath(); // Ini akan menghapus path yang kadaluarsa di dalam GetBestPath
        if (!it->second.HasValidPath()) {
            NS_LOG_DEBUG("Purging multipath entry for " << it->first);
            it = m_multipathTable.erase(it);
        } else {
            ++it;
        }
    }
}

// ==================== MULTIPATH ROUTE ENTRY METHODS ====================




void
MultipathRouteEntry::AddPath(const MultipathRouteEntry::PathInfo& pathInfo)
{
    NS_LOG_FUNCTION(this << pathInfo.nextHop);
    m_paths.push_back(pathInfo);
    NS_LOG_DEBUG("Added path to " << m_destination << " via " << pathInfo.nextHop << " with BLE metrics");
}

} // namespace aodv
} // namespace ns3
