/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
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
 * Author: Ilya Moiseenko <iliamo@cs.ucla.edu>
 */

#include "ndn-consumer-diffusion.h"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"

#include "ns3/ndn-app-face.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-data.h"

NS_LOG_COMPONENT_DEFINE ("ndn.ConsumerDiffusion");

namespace ns3 {
namespace ndn {
    
NS_OBJECT_ENSURE_REGISTERED (ConsumerDiffusion);
    
TypeId
ConsumerDiffusion::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ConsumerDiffusion")
    .SetGroupName ("Ndn")
    .SetParent<Consumer> ()
    .AddConstructor<ConsumerDiffusion> ()

    .AddAttribute ("InitialFrequency", "Initial frequency of interest packets",
                   StringValue ("1.0"),
                   MakeDoubleAccessor (&ConsumerDiffusion::m_frequency),
                   MakeDoubleChecker<double> ())
    
    .AddAttribute ("Randomize", "Type of send time randomization: none (default), uniform, exponential",
                   StringValue ("none"),
                   MakeStringAccessor (&ConsumerDiffusion::SetRandomize, &ConsumerDiffusion::GetRandomize),
                   MakeStringChecker ())

    .AddAttribute ("MaxSeq",
                   "Maximum sequence number to request",
                   IntegerValue (std::numeric_limits<uint32_t>::max ()),
                   MakeIntegerAccessor (&ConsumerDiffusion::m_seqMax),
                   MakeIntegerChecker<uint32_t> ())

    ;

  return tid;
}
    
ConsumerDiffusion::ConsumerDiffusion ()
  : m_frequency (1.0)
  , m_firstTime (true)
  , m_random (0)
  , m_rate(1000000)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_seqMax = std::numeric_limits<uint32_t>::max ();
}

ConsumerDiffusion::~ConsumerDiffusion ()
{
  if (m_random)
    delete m_random;
}

void
ConsumerDiffusion::ScheduleNextPacket ()
{
  // double mean = 8.0 * m_payloadSize / m_desiredRate.GetBitRate ();
  // std::cout << "next: " << Simulator::Now().ToDouble(Time::S) + mean << "s\n";

  if (m_firstTime)
    {
      m_sendEvent = Simulator::Schedule (Seconds (0.0),
                                         &Consumer::SendPacket, this);
      m_firstTime = false;
    }
  else if (!m_sendEvent.IsRunning ())
    m_sendEvent = Simulator::Schedule (
                                       (m_random == 0) ?
                                         Seconds(1.0 / m_frequency)
                                       :
                                         Seconds(m_random->GetValue ()),
                                       &Consumer::SendPacket, this);
}

void
ConsumerDiffusion::SetRandomize (const std::string &value)
{
  if (m_random)
    delete m_random;

  if (value == "uniform")
    {
      m_random = new UniformVariable (0.0, 2 * 1.0 / m_frequency);
    }
  else if (value == "exponential")
    {
      m_random = new ExponentialVariable (1.0 / m_frequency, 50 * 1.0 / m_frequency);
    }
  else
    m_random = 0;

  m_randomType = value;  
}

std::string
ConsumerDiffusion::GetRandomize () const
{
  return m_randomType;
}


///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
ConsumerDiffusion::OnData (const Ptr<const Data> &contentObject,
                               const Ptr<const Packet> &payload)
{
  Consumer::OnData (contentObject); // tracing inside
}

// void
// Consumer::OnNack (const Ptr<const Interest> &interest)
// {
//   Consumer::OnNack (interest); // tracing inside
// }

void
ConsumerDiffusion::SetDesiredRate (DataRate rate)
{
    m_rate = rate;
}

DataRate
ConsumerDiffusion::GetDesiredRate () const
{
    return m_rate;
}


} // namespace ndn
} // namespace ns3
