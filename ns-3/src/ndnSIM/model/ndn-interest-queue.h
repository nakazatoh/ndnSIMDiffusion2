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

#ifndef INTEREST_QUEUE_H
#define INTEREST_QUEUE_H

//#include "ns3/ndn-pit.h"
//#include "ns3/ndn-pit-entry.h"
#include "ns3/ndn-interest.h"
//#include "ns3/ndn-face.h"
#include "ns3/ndn-delayed-interest.h"
#include <queue>
#include "ns3/packet.h"
#include "ns3/object.h"
#include "ns3/traced-callback.h"

namespace ns3 {

class TraceContainer;

namespace ndn {
/**
 * \brief A FIFO queue for Interests
 */

class InterestQueue : public Object {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief InterestQueue Constructor
   *
   * Creates an Interest queue.
   */
  InterestQueue ();

  ~InterestQueue();

  /**
   * \return true if the queue is empty; false otherwise
   */
  bool IsEmpty (void) const;

  /**
   * Place an Interest and other values into the rear of the queue
   * \param di a pointer to the DelayedInterest instance to be queued
   * \return True if the operation was successful; false otherwise
   */
  bool Enqueue(Ptr<DelayedInterest> di);

  /**
   * Remove a pointer to the Interest from the front of the queue
   * \retrun 0 if the operation was not successful; the pointer to the 
   * DelayedInterest instance otherwise
   */
  Ptr<DelayedInterest> Dequeue(void);

  /**
   * Get a copy of the pointer at the front of the queue without removing it
   * \return 0 if the operation was not successful; the pointer otherwise
   */
  Ptr<const DelayedInterest> Peek (void) const;

  /**
   * Flush the queue
   */
  void DequeueAll (void);

  /**
   * \return the number of Interests currently stored in the queue
   */
  uint32_t GetNInterest (void) const;

  /**
   * \return The total number of Interest received by this queue since the 
   * simulation began, or since ResetStatistics was called, according to
   * whichever happened more recently
   */
  uint32_t GetTotalReceivedInterests (void) const;

  /**
   * Reset the counts for dropped packets and received packets.
   */
  void ResetStatistics (void);

  /**
   * @brief Remove the DelayedInterest instance specified by the specified interest
   * @param Pointer to the interest to be removed
   * @return true if successfully remove the DelayedInterest instance
   */
  bool RemoveInterest(Ptr<const Interest> interest);

private:
  /// Traced callback: fired when an Interest is enqueued
  TracedCallback<Ptr<const DelayedInterest> > m_traceEnqueue;
  /// Traced callback: fired when an Interest is dequeued
  TracedCallback<Ptr<const DelayedInterest> > m_traceDequeue;

  std::list<Ptr<DelayedInterest> > m_delayed_interests; //!< the Interests in the queue
  // uint32_t m_nInterests;              //!< Number of Interests in the queue
  uint32_t m_nTotalReceivedInterests; //!< Total received Interests
};

} // namespace ndn
} // namespace ns3

#endif /* INTEREST_QUEUE_H */
