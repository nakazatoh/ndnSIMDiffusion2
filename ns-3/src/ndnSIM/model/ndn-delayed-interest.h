/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */

#ifndef NDN_DELAYED_INTEREST_H
#define NDN_DELAYED_INTEREST_H

#include "ns3/simple-ref-count.h"
#include "ns3/ptr.h"
//#include "ns3/ndn-face.h"
//#include "ns3/ndn-pit-entry.h"
//#include "ns3/ndn-interest.h"
// #include "ns3/ndn-forwarding-strategy.h"

namespace ns3 {
namespace ndn {
  class Face;
  class Interest;
  class ForwardingStrategy;

namespace pit {
  class Entry;
}

class DelayedInterest : public SimpleRefCount<DelayedInterest>
{
  public:
    /**
     * Constructor of DelayedInterest
     * \param inFace a pointer to the incoming face of the Interest
     * \param outFace a pointer to the outgoing face of the Interest
     * \param interest a pointer to the Interest
     * \param pitEntry a pointer to the pitEntry for the Interest
     */
    DelayedInterest(Ptr<Face> inFace, Ptr<Face> outFace, Ptr<Interest> interest, Ptr<pit::Entry> pitEntry);
    
    DelayedInterest();
    ~DelayedInterest();
    Ptr<Face> m_inFace;
    Ptr<Face> m_outFace;
    Ptr<Interest> m_interest;
    Ptr<pit::Entry> m_pitEntry;
    Ptr<ForwardingStrategy> m_fs;
};

}
}

#endif // NDN_DELAYED_INTEREST_H