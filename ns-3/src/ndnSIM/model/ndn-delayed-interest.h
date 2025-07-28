/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */

#ifndef NDN_DELAYED_INTEREST_H
#define NDN_DELAYED_INTEREST_H

//#include "ns3/ptr.h"
//#include "ns3/ndn-face.h"
//#include "ns3/ndn-pit-entry.h"
//#include "ns3/ndn-interest.h"

namespace ns3 {
namespace ndn {
  class Face;
  class Interest;

namespace pit {
  class Entry;
}

  struct DelayedInterest : Object
  {
    Ptr<Face> inFace;
    Ptr<Face> outFace;
    Ptr<Interest> interest;
    Ptr<pit::Entry> pitEntry;
  };
}
}

#endif // NDN_DELAYED_INTEREST_H