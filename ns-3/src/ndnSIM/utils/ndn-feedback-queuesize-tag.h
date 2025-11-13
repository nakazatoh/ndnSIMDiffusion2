/*
 * ndn-feedback-queue-tag.h
 *
 *  Created on: 2025年11月9日
 *      Author: Nakazato
 */

#ifndef NDN_FEEDBACK_QUEUESIZE_TAG_H_
#define NDN_FEEDBACK_QUEUESIZE_TAG_H_

#include "ns3/tag.h"

namespace ns3 {
namespace ndn {

/**
 * @brief Packet tag that is used to track hop count for Interest-Data pairs
 */
class FeedbackQueueSizeTag : public Tag
{
public:
  static TypeId
  GetTypeId (void);

  /**
   * @brief Default constructor
   */
  FeedbackQueueSizeTag ()
    : m_queueSize(0)
    { };
  
  /**
   * @brief Destructor
   */
  ~FeedbackQueueSizeTag () { }

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
  double m_queueSize;
};

} // namespace ndn
} // namespace ns3



#endif /* NDN_FW_FEEDBACK_PITSIZE_TAG_H_ */

