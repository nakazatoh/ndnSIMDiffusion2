#include "ns3/log.h"
#include "ns3/ptr.h"
#include "ns3/ndn-face.h"
#include "ns3/ndn-pit-entry.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-delayed-interest.h"
#include "ns3/ndn-forwarding-strategy.h"

NS_LOG_COMPONENT_DEFINE ("ndn.DelayedInterest");

namespace ns3 {
namespace ndn {
  DelayedInterest::DelayedInterest(Ptr<Face> inFace, 
                                   Ptr<Face> outFace,
                                   Ptr<Interest> interest, 
                                   Ptr<pit::Entry> pitEntry)
                                   :
    m_inFace (inFace),
    m_outFace (outFace),
    m_interest (interest),
    m_pitEntry (pitEntry)
  {
    NS_LOG_DEBUG("Created with parameters");
  }

  DelayedInterest::DelayedInterest()
  {
    NS_LOG_DEBUG("Created");
  }

  DelayedInterest::~DelayedInterest()
  {
    NS_LOG_DEBUG("Deleted");
  }
}
}