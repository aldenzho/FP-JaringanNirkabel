/*
 * Copyright (c) 2009 IITP RAS
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Pavel Boyko <boyko@iitp.ru>, written after OlsrHelper by Mathieu Lacage
 * <mathieu.lacage@sophia.inria.fr>
 */
#include "aodv-helper.h"
#include "ns3/aodv-routing-protocol.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/names.h"
#include "ns3/node-list.h"
#include "ns3/ptr.h"
#include "ns3/boolean.h"
#include "ns3/double.h"

namespace ns3
{

AodvHelper::AodvHelper()
    : Ipv4RoutingHelper()
{
    m_enableMultipath = false; // Inisialisasi di body constructor
    m_agentFactory.SetTypeId("ns3::aodv::RoutingProtocol");
}

AodvHelper*
AodvHelper::Copy() const
{
    return new AodvHelper(*this);
}
//==== PENAMBAHN MULTIPATH ============
void
AodvHelper::SetMultipathEnabled(bool enable)
{
  m_enableMultipath = enable;
}

Ptr<Ipv4RoutingProtocol>
AodvHelper::Create(Ptr<Node> node) const
{
    Ptr<aodv::RoutingProtocol> agent = m_agentFactory.Create<aodv::RoutingProtocol>();
    node->AggregateObject(agent);

    // ====================== PENAMBAHAN MULTIPATH ==========================
    if (m_enableMultipath) {
        agent->SetMultipathEnabled(true);
    }
    return agent;
}

void
AodvHelper::Set(std::string name, const AttributeValue& value)
{

    m_agentFactory.Set(name, value);
}

int64_t
AodvHelper::AssignStreams(NodeContainer c, int64_t stream)
{
    int64_t currentStream = stream;
    Ptr<Node> node;
    for (auto i = c.Begin(); i != c.End(); ++i)
    {
        node = (*i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        NS_ASSERT_MSG(ipv4, "Ipv4 not installed on node");
        Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol();
        NS_ASSERT_MSG(proto, "Ipv4 routing not installed on node");
        Ptr<aodv::RoutingProtocol> aodv = DynamicCast<aodv::RoutingProtocol>(proto);
        if (aodv)
        {
            currentStream += aodv->AssignStreams(currentStream);
            continue;
        }
        // Aodv may also be in a list
        Ptr<Ipv4ListRouting> list = DynamicCast<Ipv4ListRouting>(proto);
        if (list)
        {
            int16_t priority;
            Ptr<Ipv4RoutingProtocol> listProto;
            Ptr<aodv::RoutingProtocol> listAodv;
            for (uint32_t i = 0; i < list->GetNRoutingProtocols(); i++)
            {
                listProto = list->GetRoutingProtocol(i, priority);
                listAodv = DynamicCast<aodv::RoutingProtocol>(listProto);
                if (listAodv)
                {
                    currentStream += listAodv->AssignStreams(currentStream);
                    break;
                }
            }
        }
    }
    return (currentStream - stream);
}

// ================== PENAMBAHAN BLE-MAODV ================ 
void
AodvHelper::EnableBLEMAODV(bool enable)
{
    m_agentFactory.Set("EnableMultipath", BooleanValue(enable));
}

void
AodvHelper::SetInitialEnergy(double energy)
{
    // Skip dulu - tidak ada attribute InitialEnergy
}

} // namespace ns3
