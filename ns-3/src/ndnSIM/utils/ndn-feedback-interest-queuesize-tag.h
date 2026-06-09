/*
 * ndn-feedback-interest-queuesize-tag.h
 *
 *  Created on: 2026年3月9日
 *      Author: Nakazato
 */

#ifndef NDN_FEEDBACK_INTEREST_QUEUESIZE_TAG_H_
#define NDN_FEEDBACK_INTEREST_QUEUESIZE_TAG_H_

#include "ns3/tag.h"

namespace ns3 {
namespace ndn {

/**
 * @brief Packet tag that is used to track hop count for Interest-Data pairs
 */
class FeedbackInterestQueueSizeTag : public Tag
{
public:
  static TypeId
  GetTypeId (void);

  /**
   * @brief Default constructor
   */
  FeedbackInterestQueueSizeTag ()
    : m_interestQueueSize(0)
    { };
  
  /**
   * @brief Destructor
   */
  ~FeedbackInterestQueueSizeTag () { }

  /**
   * @brief Set values of feedback
   */

  void
  SetQueueSize(uint32_t queueSize);

  uint32_t
  GetQueueSize() const;

  ////////////////////////////////////////////////////////
  // from ObjectBase
  ////////////////////////////////////////////////////////
  virtual TypeId
  GetInstanceTypeId () const;

  ////////////////////////////////////////////////////////
  // from Tag
  ////////////////////////////////////////////////////////
/**/
  virtual uint32_t
  GetSerializedSize () const;

  virtual void
  Serialize (TagBuffer i) const;

  virtual void
  Deserialize (TagBuffer i);

  virtual void
  Print (std::ostream &os) const;

private:
  double m_interestQueueSize;
};

} // namespace ndn
} // namespace ns3



#endif /* NDN_FEEDBACK_INTEREST-QUEUESIZE_TAG_H_ */

