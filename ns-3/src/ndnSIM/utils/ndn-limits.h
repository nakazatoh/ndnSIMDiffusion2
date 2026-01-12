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

#ifndef _NDN_LIMITS_H_
#define	_NDN_LIMITS_H_

#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/traced-value.h"
#include "ns3/ndn-face.h"
#include "ns3/ndn-interest-queue.h"

namespace ns3 {
namespace ndn {

/**
 * \ingroup ndn-fw
 * \brief Abstract class to manage Interest limits 
 */
class Limits :
    public Object
{
public:
  typedef Callback<void> CallbackHandler;

  static TypeId
  GetTypeId ();

  /**
   * @brief Default constructor
   */
  Limits ();

  /**
   * @brief Virtual destructor
   */
  virtual
  ~Limits () {}
  
  /**
   * @brief Set limit for the number of outstanding interests
   * @param rate   Maximum rate that needs to be enforced
   * @param delay  Maximum delay for BDP product for window-based limits
   */
  virtual void
  SetLimits (double rate, double delay)
  {
    m_maxRate = rate;
    m_rtt = m_maxDelay = delay;
  }    

  /**
   * @brief Get maximum rate that needs to be enforced
   */
  virtual double
  GetMaxRate () const
  {
    return m_maxRate;
  }

  /**
   * @brief Get maximum delay for BDP product for window-based limits
   */
  virtual double
  GetMaxDelay () const
  {
    return m_maxDelay;
  }

  /**
   * @brief Get maximum limit (interpretation of the limit depends on realization)
   */
  virtual
  double
  GetMaxLimit () const = 0;

  /**
   * @brief Check whether limits are enabled or not
   */
  virtual inline bool
  IsEnabled () const
  {
    return m_maxRate > 0.0;
  }

  /**
   * @brief Update a current value of the limit
   * @param limit Value of current limit.
   *
   * Note that interpretation of this value may be different in different ndn::Limit realizations
   *
   * All realizations will try to guarantee that if limit is larger than previously set value of maximum limit,
   * then the current limit will be limited to that maximum value
   */
  virtual void
  UpdateCurrentLimit (double limit) = 0;

  /**
   * @brief Get value of the current limit
   *
   * Note that interpretation of this value may be different in different ndn::Limit realizations
   */
  virtual double
  GetCurrentLimit () const = 0;

  /**
   * @brief Get value of the current limit in terms of maximum rate that needs to be enforced
   *
   * Compared to GetCurrentLimit, this method guarantees that the returned value is maximum number of packets
   * that can be send out within one second (max "rate")
   */
  virtual double
  GetCurrentLimitRate () const = 0;
  
  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////

  /**
   * @brief Realization-specific method called to check availability of the limit
   */
  virtual bool
  IsBelowLimit () = 0;

  /**
   * @brief "Borrow" limit
   *
   * IsBelowLimit **must** be true, otherwise assert fail
   */
  virtual void
  BorrowLimit () = 0;

  /**
   * @brief "Return" limit
   */
  virtual void
  ReturnLimit () = 0;

  /**
   * @brief Set link delay (in seconds)
   *
   * This is a supplementary information that may or may not be useful for limits
   */
  virtual void
  SetLinkDelay (double delay)
  {
    m_linkDelay = delay;
  }

  /**
   * @brief Get link delay (in seconds)
   */
  virtual double
  GetLinkDelay () const
  {
    return m_linkDelay;
  }

  /**
   * @brief Enqueue to the interest queue
   * @param di a pointer to a DelayedInterest
   */
  void
  Enqueue (Ptr<DelayedInterest> di);

  /**
   * @brief Dequeue from the interest queue
   * @return the first element in the queue
   */
  Ptr<DelayedInterest>
  Dequeue ();

  /**
   * @brief Read the front of the interest queue
   * @return the first element in the queue
   */
  Ptr<const DelayedInterest>
  Peek ();

  /**
   * @brief Get current queue length
   */
  uint32_t
  GetQueueLength();

  /**
   * @brief Remove the specified interest from the interest queue
   * @param Interest to be removed
   * @return true if the interest exists in the queue and removed
   */
  bool
  RemoveInterest(Ptr<const Interest> interest);

  /**
   * @brief Set m_nodeId of this Limits instance
   */
  virtual void
  SetNodeId(uint32_t nodeId);

  /**
   * @brief Get m_nodeId of this Limits instance
   */
  virtual uint32_t
  GetNodeId();

  /**
   * @brief Set m_f_qSize of this Limits instance
   * @param f_qSize the max of interest queue size and data queue size of upstream node
   */
  virtual void SetFQSize(double f_qSize);

  /**
   * @brief Get m_f_qSize of this Limits instance
   * @return the value of m_f_qSize
   */
  virtual double GetFQSize();

  /**
   * @brief Incorporate an observed RTT to the average RTT of this Limits instance
   * @param rtt an observed RTT value
   */
  virtual void AddRTT(double rtt);

  /**
   * @brief Read average RTT of this Limits instance
   * @return the average RTT value
   */
  virtual double GetRTT();

  /**
   * @brief Set update mode of current limits
   * @param mode update mode 0: rtt 1: fixed delay
   */
  void SetUpdateMode(uint32_t mode);
  
  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////

  /**
   * @brief Set callback which will be called when exhausted limit gets a new slot
   */
  void
  RegisterAvailableSlotCallback (CallbackHandler handler);

protected:
  void
  FireAvailableSlotCallback ();

  InterestQueue m_iq;
  uint32_t m_nodeId;
  double m_f_qSize;
  double m_rtt;
  Time m_nxtAdjTime {Seconds(0.0)}; 
  uint32_t m_updateMode {0}; // 0: rtt, 1: delay

private:
  double m_maxRate;
  double m_maxDelay;

  CallbackHandler m_handler;

  double m_linkDelay;
};

} // namespace ndn
} // namespace ns3

#endif // _NDN_LIMITS_H_
