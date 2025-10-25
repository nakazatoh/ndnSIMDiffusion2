/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
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
 */

#include "ns3/log.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/ndn-interest-queue.h"

NS_LOG_COMPONENT_DEFINE ("InterestQueue");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (InterestQueue);

TypeId InterestQueue::GetTypeId (void) 
{
  static TypeId tid = TypeId ("ns3::ndn::InterestQueue")
    .SetParent<Object> ()
    .AddConstructor<InterestQueue> ()
    .AddTraceSource ("Enqueue", "Enqueue an Interest in the queue.",
                     MakeTraceSourceAccessor (&InterestQueue::m_traceEnqueue))
    .AddTraceSource ("Dequeue", "Dequeue an Interest in the queue.",
                     MakeTraceSourceAccessor (&InterestQueue::m_traceDequeue))
  ;
  return tid;
}

InterestQueue::InterestQueue () :
  // m_nInterests (0),
  m_nTotalReceivedInterests (0)
{
  NS_LOG_FUNCTION (this);
}

InterestQueue::~InterestQueue ()
{
  NS_LOG_FUNCTION (this);
}

bool 
InterestQueue::Enqueue (Ptr<DelayedInterest> di)
{
  NS_LOG_FUNCTION (this << di);

  m_traceEnqueue(di);

  // m_nInterests++;
  m_nTotalReceivedInterests++;
  m_delayed_interests.push_back (di);

  NS_LOG_LOGIC ("Number Interests " << m_delayed_interests.size());

  return true;
}

Ptr<DelayedInterest>
InterestQueue::Dequeue (void)
{
  NS_LOG_FUNCTION (this);

  if (m_delayed_interests.empty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  Ptr<DelayedInterest> di = m_delayed_interests.front ();
  m_delayed_interests.pop_front ();

  // m_nInterests--;
  NS_LOG_LOGIC ("Popped " << di);
  m_traceDequeue (di);
  NS_LOG_LOGIC ("Number Interests " << m_delayed_interests.size ());
  
  return di;
}

Ptr<const DelayedInterest>
InterestQueue::Peek (void) const
{
  NS_LOG_FUNCTION (this);

  if (m_delayed_interests.empty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  Ptr<DelayedInterest> di = m_delayed_interests.front ();

  NS_LOG_LOGIC ("Number Interests " << m_delayed_interests.size ());

  return di;
}

void
InterestQueue::DequeueAll (void)
{
  NS_LOG_FUNCTION (this);
  while (!IsEmpty ())
  {
    Dequeue();
  }
}

bool
InterestQueue::IsEmpty (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("returns " << (m_delayed_interests.size() == 0));
  return m_delayed_interests.size() == 0; //m_nInterests == 0;
}

uint32_t
InterestQueue::GetNInterest (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("returns " << m_delayed_interests.size());
  return m_delayed_interests.size(); // m_nInterests;
}

uint32_t
InterestQueue::GetTotalReceivedInterests (void) const
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("returns " << m_nTotalReceivedInterests);
  return m_nTotalReceivedInterests;
}

void
InterestQueue::ResetStatistics (void)
{
  NS_LOG_FUNCTION (this);
  // m_nInterests = 0;
  m_nTotalReceivedInterests = 0;
}

bool 
InterestQueue::RemoveInterest(Ptr<const Interest> interest)
{
  NS_LOG_FUNCTION (this << "interest:" << interest->GetName());
  for (std::list<Ptr<DelayedInterest> >::iterator dii = m_delayed_interests.begin(); 
    dii != m_delayed_interests.end(); dii++)
    {
      if ((*dii)->m_interest == interest)
      {
        m_delayed_interests.erase(dii);
        return true;
      }
    }
  return false;
}


} // namespace ndn
} // namespace ns3

