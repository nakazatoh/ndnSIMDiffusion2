#include "ns3/log.h"
#include "ns3/ptr.h"
#include "ns3/ndn-face.h"
#include "ns3/ndn-pit-entry.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-delayed-interest.h"

NS_LOG_COMPONENT_DEFINE ("ndn.DelayedInterest");

namespace ns3 {
namespace ndn {
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