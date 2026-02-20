/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2011 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#include "ndn-limits.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/random-variable.h"

NS_LOG_COMPONENT_DEFINE ("ndn.Limits");

namespace ns3 {
namespace ndn {

TypeId
Limits::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::ndn::Limits")
    .SetGroupName ("Ndn")
    .SetParent <Object> ()
    
    ;
  return tid;
}

Limits::Limits ()
  : m_maxRate (-1)
  , m_maxDelay (1.0)
  , m_handler (MakeNullCallback<void> ())
  , m_linkDelay (0)
  , m_f_qSize (0.0)
  , m_rtt (1.0)
  , m_rttDev (0.0)
{
}


void
Limits::RegisterAvailableSlotCallback (CallbackHandler handler)
{
  m_handler = handler;
}

void
Limits::FireAvailableSlotCallback ()
{
  if (!m_handler.IsNull ())
    m_handler ();
}
  void
  Limits::Enqueue (Ptr<DelayedInterest> di)
  {
    m_iq.Enqueue(di);
    NS_LOG_DEBUG("queue size: " << m_iq.GetNInterest());
  }
  
  Ptr<DelayedInterest>
  Limits::Dequeue ()
  {
    Ptr<DelayedInterest> di = m_iq.Dequeue();
    if (di != 0)
      NS_LOG_DEBUG("queue size: " << m_iq.GetNInterest());
    else
      NS_LOG_DEBUG("queue size: 0");
    
    return di;
  }
  
  Ptr<const DelayedInterest>
  Limits::Peek ()
  {
    Ptr<const DelayedInterest> di = m_iq.Peek();
    if (di != 0)
      NS_LOG_DEBUG("queue size: " << m_iq.GetNInterest());
    else
      NS_LOG_DEBUG("queue size: 0");
    
    return di;
  }

  uint32_t
  Limits::GetQueueLength()
  {
    return m_iq.GetNInterest();
  }

  bool
  Limits::RemoveInterest(Ptr<const Interest> interest)
  {
    return m_iq.RemoveInterest(interest);
  }

  void
  Limits::SetNodeId(uint32_t nodeId)
  {
    m_nodeId = nodeId;
  }

  uint32_t
  Limits::GetNodeId()
  {
    return m_nodeId;
  }

  void
  Limits::SetFQSize(double f_qSize)
  {
    NS_LOG_FUNCTION(this << "f_qSize:" << f_qSize);
    m_f_qSize = f_qSize;
  }

  double
  Limits::GetFQSize()
  {
    return m_f_qSize;
  }

  void
  Limits::AddRTT(double rtt)
  {
    NS_LOG_FUNCTION(this << "rtt:" << rtt);
    double dev = rtt - m_rtt;
    m_rtt = m_rtt * 0.8 + rtt * 0.2;
    m_rttDev += 0.1 * (std::abs(dev) - m_rtt);
  }

  double
  Limits::GetRTT()
  {
    return m_rtt;
  }

  void
  Limits::SetUpdateMode(uint32_t mode)
  {
    m_updateMode = mode;
  }

} // namespace ndn
} // namespace ns3
