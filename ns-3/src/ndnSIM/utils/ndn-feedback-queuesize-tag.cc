/*
 * ndn-feedback-queuesize-tag.cc
 *
 *  Created on: 2025年11月9日
 *      Author: Hidenori Nakazato
 */

#include "ndn-feedback-queuesize-tag.h"

namespace ns3 {
namespace ndn {

TypeId
FeedbackQueueSizeTag::GetTypeId ()
{
  static TypeId tid = TypeId("ns3::ndn::FeedbackQueueSizeTag")
    .SetParent<Tag>()
    .AddConstructor<FeedbackQueueSizeTag>()
    ;
  return tid;
}

TypeId
FeedbackQueueSizeTag::GetInstanceTypeId () const
{
  return FeedbackQueueSizeTag::GetTypeId ();
}

uint32_t
FeedbackQueueSizeTag::GetSerializedSize () const
{
  return sizeof(uint32_t);
}

void
FeedbackQueueSizeTag::Serialize (TagBuffer i) const
{
  i.WriteU32 (m_queueSize);
}
  
void
FeedbackQueueSizeTag::Deserialize (TagBuffer i)
{
  m_queueSize = i.ReadU32 ();
}

void
FeedbackQueueSizeTag::SetQueueSize(uint32_t queueSize)
{
  m_queueSize = queueSize;
}

uint32_t
FeedbackQueueSizeTag::GetQueueSize() const
{
  return m_queueSize;
}

void
FeedbackQueueSizeTag::Print (std::ostream &os) const
{
  os << m_queueSize;
}

} // namespace ndn
} // namespace ns3





