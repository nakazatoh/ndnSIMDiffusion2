/*
 * ndn-feedback-interest-queuesize-tag.cc
 *
 *  Created on: 2026年3月9日
 *      Author: Hidenori Nakazato
 */

#include "ndn-feedback-interest-queuesize-tag.h"

namespace ns3 {
namespace ndn {

TypeId
FeedbackInterestQueueSizeTag::GetTypeId ()
{
  static TypeId tid = TypeId("ns3::ndn::FeedbackInterestQueueSizeTag")
    .SetParent<Tag>()
    .AddConstructor<FeedbackInterestQueueSizeTag>()
    ;
  return tid;
}

TypeId
FeedbackInterestQueueSizeTag::GetInstanceTypeId () const
{
  return FeedbackInterestQueueSizeTag::GetTypeId ();
}

uint32_t
FeedbackInterestQueueSizeTag::GetSerializedSize () const
{
  return sizeof(uint32_t);
}

void
FeedbackInterestQueueSizeTag::Serialize (TagBuffer i) const
{
  i.WriteU32 (m_interestQueueSize);
}
  
void
FeedbackInterestQueueSizeTag::Deserialize (TagBuffer i)
{
  m_interestQueueSize = i.ReadU32 ();
}

void
FeedbackInterestQueueSizeTag::SetQueueSize(uint32_t queueSize)
{
  m_interestQueueSize = queueSize;
}

uint32_t
FeedbackInterestQueueSizeTag::GetQueueSize() const
{
  return m_interestQueueSize;
}

void
FeedbackInterestQueueSizeTag::Print (std::ostream &os) const
{
  os << m_interestQueueSize;
}

} // namespace ndn
} // namespace ns3





