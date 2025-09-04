/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
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
 * Author:  Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *          Ilya Moiseenko <iliamo@cs.ucla.edu>
 */

#include "ndn-forwarding-strategy.h"
///////////////////////////////////////////
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
///////////////////////////////////////////
#include "ns3/ndn-pit.h"
#include "ns3/ndn-pit-entry.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-data.h"
#include "ns3/ndn-pit.h"
#include "ns3/ndn-fib.h"
#include "ns3/ndn-content-store.h"
#include "ns3/ndn-face.h"

#include "ns3/assert.h"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/boolean.h"
#include "ns3/string.h"


#include "ns3/ndnSIM/utils/ndn-fw-hop-count-tag.h"
/////////////////////////////////////////////////////////////////
#include "ns3/ndnSIM/utils/ndn-fw-feedback-pitsize-tag.h"
#include "ns3/ndnSIM/utils/ndn-fw-feedback-pitsize-difference-tag.h"
#include "ns3/ndnSIM/utils/ndn-fw-feedback-rate-tag.h"
/////////////////////////////////////////////////////////////////
#include <boost/ref.hpp>
#include <boost/foreach.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/tuple/tuple.hpp>
#include <iostream>

NS_LOG_COMPONENT_DEFINE ("ndn.ForwardingStrategy");

namespace ll = boost::lambda;

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (ForwardingStrategy);

std::string
ForwardingStrategy::GetLogName ()
{
  return "ndn.fw";
}

TypeId ForwardingStrategy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ForwardingStrategy")
    .SetGroupName ("Ndn")
    .SetParent<Object> ()

    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////

    .AddTraceSource ("OutInterests",  "OutInterests",  MakeTraceSourceAccessor (&ForwardingStrategy::m_outInterests))
    .AddTraceSource ("InInterests",   "InInterests",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inInterests))
    .AddTraceSource ("DropInterests", "DropInterests", MakeTraceSourceAccessor (&ForwardingStrategy::m_dropInterests))

    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////

    .AddTraceSource ("OutData",  "OutData",  MakeTraceSourceAccessor (&ForwardingStrategy::m_outData))
    .AddTraceSource ("InData",   "InData",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inData))
    .AddTraceSource ("DropData", "DropData", MakeTraceSourceAccessor (&ForwardingStrategy::m_dropData))

    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////

    .AddTraceSource ("SatisfiedInterests",  "SatisfiedInterests",  MakeTraceSourceAccessor (&ForwardingStrategy::m_satisfiedInterests))
    .AddTraceSource ("TimedOutInterests",   "TimedOutInterests",   MakeTraceSourceAccessor (&ForwardingStrategy::m_timedOutInterests))

    .AddAttribute ("CacheUnsolicitedDataFromApps", "Cache unsolicited data that has been pushed from applications",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ForwardingStrategy::m_cacheUnsolicitedDataFromApps),
                   MakeBooleanChecker ())
    
    .AddAttribute ("CacheUnsolicitedData", "Cache overheard data that have not been requested",
                   BooleanValue (false),
                   MakeBooleanAccessor (&ForwardingStrategy::m_cacheUnsolicitedData),
                   MakeBooleanChecker ())

    .AddAttribute ("DetectRetransmissions", "If non-duplicate interest is received on the same face more than once, "
                                            "it is considered a retransmission",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ForwardingStrategy::m_detectRetransmissions),
                   MakeBooleanChecker ())

    .AddAttribute ("SelectSatPI", "Select SatisfyPendingInterestXXXX, 0:original 1:DTCC 2:QSF",
                   UintegerValue(0),
                   MakeUintegerAccessor(&ForwardingStrategy::m_selectSatPI),
                   MakeUintegerChecker<uint32_t>(0, 2))
    ;
  return tid;
}
///////////////////////////////////////////////
ForwardingStrategy::ForwardingStrategy () :
  ad (5.0),
  m_interestRateTable {},
  m_consumerNeighbour (false)
{
}
//////////////////////////////////////////////
ForwardingStrategy::~ForwardingStrategy ()
{
}

void
ForwardingStrategy::NotifyNewAggregate ()
{
  if (m_pit == 0)
    {
      m_pit = GetObject<Pit> ();
    }
  if (m_fib == 0)
    {
      m_fib = GetObject<Fib> ();
    }
  if (m_contentStore == 0)
    {
      m_contentStore = GetObject<ContentStore> ();
    }

  Object::NotifyNewAggregate ();
}

void
ForwardingStrategy::DoDispose ()
{
  m_pit = 0;
  m_contentStore = 0;
  m_fib = 0;

  Object::DoDispose ();
}

void
ForwardingStrategy::OnInterest (Ptr<Face> inFace,
                                Ptr<Interest> interest)
{
        NS_LOG_FUNCTION (inFace << interest->GetName ());
        m_inInterests (interest, inFace);

///////////////////////////////////////////////////////////////////
  //  can see how to move packet
  double tm = Simulator::Now ().ToDouble (Time::S);

  Ptr<Node> node = inFace -> GetNode();
  uint32_t nodeID = node -> GetId();
  Ptr<Limits> faceLimits = inFace -> GetObject<Limits>();
  // double rate = faceLimits -> GetCurrentLimit();
  uint32_t faceid = inFace->GetId();
  //  if ((nodeID == 10 && faceid == 3) || nodeID == 14 || nodeID == 15){
  uint32_t seq = interest->GetName ().get (-1).toSeqNum ();
  NS_LOG_LOGIC("Node: " << nodeID 
            << " interfaceID: " << faceid
            << " seq#: " << seq);
  
//////////////////////////////////////////////////////////////////
  
  FwFeedbackPitsizeTag bwFeedbackPitsizeTag;
  if (interest->GetPayload()->PeekPacketTag(bwFeedbackPitsizeTag))
  {
    double b_pitsize = bwFeedbackPitsizeTag.GetPitSize();
    inFace->SetBPitsize(b_pitsize);
    m_consumerNeighbour = false;
  }
  else 
  {
    inFace->SetBPitsize(0.0);
    m_consumerNeighbour = true;
  }
        Ptr<pit::Entry> pitEntry = m_pit->Lookup (*interest);
        bool similarInterest = true;

        if (pitEntry == 0)
        {
                similarInterest = false;
                pitEntry = m_pit->Create (interest);
                if (pitEntry != 0)
                {
                        DidCreatePitEntry (inFace, interest, pitEntry);
                }
                else
                {
                        FailedToCreatePitEntry (inFace, interest);
                        return;
                }
        }

        bool isDuplicated = true;
        if (!pitEntry->IsNonceSeen (interest->GetNonce ()))
        {
                pitEntry->AddSeenNonce (interest->GetNonce ());
                isDuplicated = false;
        }
        if (isDuplicated)
        {
                DidReceiveDuplicateInterest (inFace, interest, pitEntry);
                return;
        }

        Ptr<Data> contentObject;
        contentObject = m_contentStore->Lookup (interest);
        if (contentObject != 0)
        {
                FwHopCountTag hopCountTag;
                if (interest->GetPayload ()->PeekPacketTag (hopCountTag))
                {
                contentObject->GetPayload ()->AddPacketTag (hopCountTag);
                }

                pitEntry->AddIncoming (inFace/*, Seconds (1.0)*/);

                // Do data plane performance measurements
                WillSatisfyPendingInterest (0, pitEntry);

                // Actually satisfy pending interest
                // SatisfyPendingInterestQSF (0, contentObject, pitEntry);
                switch (m_selectSatPI)
                {
                  case 0:
                    SatisfyPendingInterest(0, contentObject, pitEntry);
                    break;
                  case 1:
                    SatisfyPendingInterestDTCC(0, contentObject, pitEntry);
                    break;
                  case 2:
                    SatisfyPendingInterestQSF(0, contentObject, pitEntry);
                    break;
                  default:
                    throw std::out_of_range("m_selectSatPI out of range");
                }
                return;
        }

        if (similarInterest && ShouldSuppressIncomingInterest (inFace, interest, pitEntry))
        {
                pitEntry->AddIncoming (inFace/*, interest->GetInterestLifetime ()*/);
                // update PIT entry lifetime
                pitEntry->UpdateLifetime (interest->GetInterestLifetime ());

                // Suppress this interest if we're still expecting data from some other face
                NS_LOG_LOGIC ("Suppress interests");
                m_dropInterests (interest, inFace);

                DidSuppressSimilarInterest (inFace, interest, pitEntry);
                return;
        }

        if (similarInterest)
        {
                DidForwardSimilarInterest (inFace, interest, pitEntry);
        }

        PropagateInterest (inFace, interest, pitEntry);
} //OnInterest close//

void
ForwardingStrategy::OnData (Ptr<Face> inFace,
                            Ptr<Data> data)
{
        NS_LOG_FUNCTION (inFace << data->GetName ());
        m_inData (data, inFace);

/////////////////////////////
Ptr<Node> node = inFace -> GetNode();
  uint32_t nodeID = node -> GetId();
  Ptr<Limits> faceLimits = inFace -> GetObject<Limits>();
  double rate = faceLimits -> GetCurrentLimit();
  
  uint32_t seq = data->GetName ().get (-1).toSeqNum ();
  NS_LOG_LOGIC("Node: " << nodeID 
            << " interfaceID: " << inFace -> GetId() 
            << " seq#: " << seq);
///////////////////////////////  

        // Lookup PIT entry
        Ptr<pit::Entry> pitEntry = m_pit->Lookup (*data);
        if (pitEntry == 0)
        {
                bool cached = false;

                if (m_cacheUnsolicitedData || (m_cacheUnsolicitedDataFromApps && (inFace->GetFlags () & Face::APPLICATION)))
                {
                        // Optimistically add or update entry in the content store
                        cached = m_contentStore->Add (data);
                }
                else
                {
                        // Drop data packet if PIT entry is not found
                        // (unsolicited data pam_f_pitiszeckets should not "poison" content store)

                        //drop dulicated or not requested data packet
                        m_dropData (data, inFace);
                }
      
                DidReceiveUnsolicitedData (inFace, data, cached);
                return;
        }
        else
        {
                bool cached = m_contentStore->Add (data);
                DidReceiveSolicitedData (inFace, data, cached);
        }

        while (pitEntry != 0)
        {
                // Do data plane performance measurements
                WillSatisfyPendingInterest (inFace, pitEntry);

                // Actually satisfy pending interest

                FwFeedbackPitsizeTag feedbackPitsizeTag;
                data->GetPayload ()->PeekPacketTag(feedbackPitsizeTag);
 

                // if (feedbackPitsizeTag.GetPitSize() >= 0)
                //{      
      	                // SatisfyPendingInterestQSF (inFace, data, pitEntry);
                switch (m_selectSatPI)
                {
                  case 0:
                    SatisfyPendingInterest(inFace, data, pitEntry);
                    break;
                  case 1:
                    SatisfyPendingInterestDTCC(inFace, data, pitEntry);
                    break;
                  case 2:
                    SatisfyPendingInterestQSF(inFace, data, pitEntry);
                    break;
                  default:
                    throw std::out_of_range("m_selectSatPI out of range");
                }
                //}
                //else
                //{
                        //SatisfyPendingInterest (inFace, data, pitEntry);
                        //std::cout << "data包的Dh已经是0了，不用调整步数"<< "\n"; 
                //}

                // Lookup another PIT entry
                pitEntry = m_pit->Lookup (*data);
        }
} //OnData close

void
ForwardingStrategy::DidCreatePitEntry (Ptr<Face> inFace,
                                       Ptr<const Interest> interest,
                                       Ptr<pit::Entry> pitEntrypitEntry)
{
}

void
ForwardingStrategy::FailedToCreatePitEntry (Ptr<Face> inFace,
                                            Ptr<const Interest> interest)
{
  m_dropInterests (interest, inFace);
}

void
ForwardingStrategy::DidReceiveDuplicateInterest (Ptr<Face> inFace,
                                                 Ptr<const Interest> interest,
                                                 Ptr<pit::Entry> pitEntry)
{
  /////////////////////////////////////////////////////////////////////////////////////////
  //                                                                                     //
  // !!!! IMPORTANT CHANGE !!!! Duplicate interests will create incoming face entry !!!! //
  //                                                                                     //
  /////////////////////////////////////////////////////////////////////////////////////////
  pitEntry->AddIncoming (inFace);
  m_dropInterests (interest, inFace);
}

void
ForwardingStrategy::DidSuppressSimilarInterest (Ptr<Face> face,
                                                Ptr<const Interest> interest,
                                                Ptr<pit::Entry> pitEntry)
{
}

void
ForwardingStrategy::DidForwardSimilarInterest (Ptr<Face> inFace,
                                               Ptr<const Interest> interest,
                                               Ptr<pit::Entry> pitEntry)
{
}

void
ForwardingStrategy::DidExhaustForwardingOptions (Ptr<Face> inFace,
                                                 Ptr<const Interest> interest,
                                                 Ptr<pit::Entry> pitEntry)
{
  NS_LOG_FUNCTION (this << boost::cref (*inFace));
  if (pitEntry->AreAllOutgoingInVain ())
    {
      m_dropInterests (interest, inFace);

      // All incoming interests cannot be satisfied. Remove them
      pitEntry->ClearIncoming ();

      // Remove also outgoing
      pitEntry->ClearOutgoing ();

      // Set pruning timout on PIT entry (instead of deleting the record)
      m_pit->MarkErased (pitEntry);
    }
}



bool
ForwardingStrategy::DetectRetransmittedInterest (Ptr<Face> inFace,
                                                 Ptr<const Interest> interest,
                                                 Ptr<pit::Entry> pitEntry)
{
  pit::Entry::in_iterator existingInFace = pitEntry->GetIncoming ().find (inFace);

  bool isRetransmitted = false;
  pit::Entry::in_iterator inFaceEnd = pitEntry->GetIncoming().end();

  if (existingInFace != pitEntry->GetIncoming ().end ())
    {
      // this is almost definitely a retransmission. But should we trust the user on that?
      isRetransmitted = true;
    }

  return isRetransmitted;
}

void
ForwardingStrategy::SatisfyPendingInterest (Ptr<Face> inFace,
                                            Ptr<Data> data,
                                            Ptr<pit::Entry> pitEntry)
{
  if (inFace == 0)
  {
    const pit::Entry::in_iterator incoming = pitEntry->GetIncoming ().begin();
    uint32_t outFace_data = incoming->m_face->GetId();
    uint32_t nodeID = incoming->m_face->GetNode()->GetId();
    uint32_t inPitsize=0;

    Ptr<pit::Entry> pitEntry2 = m_pit -> Begin(); //PITエントリの最初
    while(pitEntry2) //PITエントリを全部見る
    {
      std::set<ndn::pit::IncomingFace> incoming_face  = pitEntry2 -> GetIncoming(); //incomingFaceをみる（インタレストが入ってきたFace）
      for(std::set<ndn::pit::IncomingFace>::iterator in_itr = incoming_face.begin(); in_itr != incoming_face.end(); ++in_itr)
      {
        uint32_t tmpInfaceId;
        tmpInfaceId = in_itr -> m_face -> GetId();
        if(tmpInfaceId == outFace_data) // このデータが出て行くFaceに関するPITサイズの計算
        {
          inPitsize++;
        }
      }
      pitEntry2 = m_pit -> Next(pitEntry2);
    }  //while() close//

    FwFeedbackPitsizeTag feedbackPitsizeTag;
    FwFeedbackRateTag feedbackRateTag;

    bool pitsizeTagPresent = data->GetPayload()->PeekPacketTag(feedbackPitsizeTag);
    bool rateTagPresent = data->GetPayload()->PeekPacketTag(feedbackRateTag);

    Ptr<Packet> payloadCopy = data->GetPayload()->Copy();

    uint32_t seq = data->GetName().get(-1).toSeqNum();
    NS_LOG_DEBUG("Node: " << nodeID << " Interest-in-face: " << outFace_data 
      << " Interest-out-face: Cache" << " b_pitsize: " << incoming->m_face->GetBPitsize()
      << " pitsize_in: " << inPitsize << " pitsize_out: 1" 
      << " f_pitsize: 1" << " f_pitsize_portion: 1" << " rateLimit: NA" 
      << " seq#: " << seq << " f_rate: NA");

    if (pitsizeTagPresent)
    {
      payloadCopy->RemovePacketTag(feedbackPitsizeTag);
      payloadCopy->RemovePacketTag(feedbackRateTag);
    }
      
    feedbackPitsizeTag.SetPitSize(inPitsize);
    payloadCopy -> AddPacketTag(feedbackPitsizeTag);
    double rate = incoming->m_face->GetObject<Limits>()->GetCurrentLimit();
    feedbackRateTag.SetRate(rate);
    payloadCopy -> AddPacketTag(feedbackRateTag);

    data->SetPayload (payloadCopy);


    bool ok = incoming->m_face->SendData (data);

    DidSendOutData (inFace, incoming->m_face, data, pitEntry);
    // NS_LOG_DEBUG ("Satisfy " << *incoming->m_face);

    if (!ok)
    {
      m_dropData (data, incoming->m_face);
      NS_LOG_DEBUG ("Cannot satisfy data to " << *incoming->m_face);
    }
  }
  else
  {
    pitEntry->RemoveIncoming (inFace);
	  	
    Ptr<Node> node = inFace -> GetNode();
    uint32_t nodeID = node -> GetId();
    uint32_t infaceId = inFace -> GetId(); //データが入ってきたFaceのID

    //satisfy all pending incoming Interests
    BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
    {
    ///2018/1/4/////////making feedback////////////////
      const int max_number_of_face = 5; //ノードのとりうるFaceの最大数  

      uint32_t outFace_data = incoming.m_face -> GetId(); // このデータが出て行くfaceID 
      uint32_t inPitsize=0; //このデータが出て行くFaceのピットのサイズ。重み付けの分母。
      uint32_t outPitsize = 0;
      uint32_t totalOutPitsize = 0;

      bool ExistOutFace_data=false; //このデータが出て行くFaceに関連するPITエントリかどうかを調べるフラグ。
          //std::cout << infaceId << ",";

      Ptr<pit::Entry> pitEntry2 = m_pit -> Begin(); //PITエントリの最初

      while(pitEntry2) //PITエントリを全部見る
      {
        std::set<ndn::pit::IncomingFace> incoming_face  = pitEntry2 -> GetIncoming(); //incomingFaceをみる（インタレストが入ってきたFace）
        for(std::set<ndn::pit::IncomingFace>::iterator in_itr = incoming_face.begin(); in_itr != incoming_face.end(); ++in_itr)
        {
          uint32_t tmpInfaceId;
          tmpInfaceId = in_itr -> m_face -> GetId();
          if(tmpInfaceId == outFace_data) // このデータが出て行くFaceに関するPITサイズの計算
          {
            inPitsize++;
            ExistOutFace_data = true;
          }
        }

        std::set<ndn::pit::OutgoingFace> outgoing_face  = pitEntry2 -> GetOutgoing(); //outgoingFaceをみる（インタレストが出て行ったFace）
        for(std::set<ndn::pit::OutgoingFace>::iterator out_itr = outgoing_face.begin(); out_itr != outgoing_face.end(); ++out_itr)
        {
          uint32_t tmpOutfaceId;
          tmpOutfaceId = out_itr -> m_face -> GetId();
          if(tmpOutfaceId ==infaceId) //今回のデータが出て行くFaceに関するフィードバック情報を計算する
          {		
            if(ExistOutFace_data)
            {
              outPitsize++;
            }
            totalOutPitsize++;
          }
        }

        pitEntry2 = m_pit -> Next(pitEntry2);
        ExistOutFace_data =false;
      }  //while() close//

      double pitsize_in = inPitsize;
      double pitsize_out = outPitsize;

      /* create a new FeedbackTag */ 
      FwFeedbackPitsizeTag feedbackPitsizeTag;
      FwFeedbackRateTag feedbackRateTag;

      bool pitsizeTagPresent = data -> GetPayload() ->  PeekPacketTag(feedbackPitsizeTag);
      bool rateTagPresent = data -> GetPayload() ->  PeekPacketTag(feedbackRateTag);

      double f_pitsize = feedbackPitsizeTag.GetPitSize();
      double f_rate = feedbackRateTag.GetRate();

      inFace->SetFPitsize(f_pitsize);
      //double f_pitsizedif = pitsize_out - f_pitsize;
      double f_pitsize_portion = f_pitsize * pitsize_out / totalOutPitsize;
      double f_pitsizedif = pitsize_out - f_pitsize_portion;
      double b_pitsize = incoming.m_face->GetBPitsize();
      double b_pitsizedif = b_pitsize - pitsize_in;
      double rate;
      double newRate;

      double tm = Simulator::Now ().ToDouble (Time::S);
      Ptr<Packet> payloadOriginal = data->GetPayload()->Copy();
      Ptr<Packet> payloadCopy = payloadOriginal->Copy();           

      if (!pitsizeTagPresent) //for ndn-qsf.cc
      {
        rate = (incoming.m_face->GetObject<Limits>())->GetCurrentLimit();
        newRate = rate;
        uint32_t seq = data->GetName().get(-1).toSeqNum();

        NS_LOG_DEBUG("Node: " << nodeID << " Interest-in-face: " << outFace_data 
          << " Interest-out-face: " << infaceId << " b_pitsize: " << b_pitsize 
          << " pitsize_in: " << pitsize_in << " pitsize_out: " << pitsize_out
          << " f_pitsize: " << f_pitsize << " f_pitsize_portion: " << f_pitsize_portion
          << " rateLimit: " << rate << " seq#: " << seq
          << " f_rate: " << f_rate);
        m_interestRateTable[outFace_data][infaceId] = rate;
      }  
      else               
      {
        Ptr<Limits> faceLimits = inFace -> GetObject<Limits>();
        rate = faceLimits -> GetCurrentLimit();
        uint32_t seq = data->GetName().get(-1).toSeqNum();
        NS_LOG_DEBUG("Node: " << nodeID << " Interest-in-face: " << outFace_data 
          << " Interest-out-face: " << infaceId << " b_pitsize: " << b_pitsize 
          << " pitsize_in: " << pitsize_in << " pitsize_out: " << pitsize_out
          << " f_pitsize: " << f_pitsize << " f_pitsize_portion: " << f_pitsize_portion
          << " rateLimit: " << rate << " seq#: " << seq
          << " f_rate: " << f_rate);
         
        m_interestRateTable[outFace_data][infaceId] = rate;

        payloadCopy->RemovePacketTag(feedbackPitsizeTag);
        payloadCopy->RemovePacketTag(feedbackRateTag);

      } //if(nodeID != 19  && nodeID != 20) close

      /* set pitsize on feedback */
      feedbackPitsizeTag.SetPitSize(inPitsize);
      payloadCopy -> AddPacketTag(feedbackPitsizeTag);

      /* set rate on feedback */
      double rate_sum = 0.0;
      for (std::vector<double>::iterator v = m_interestRateTable[outFace_data].begin(); v != m_interestRateTable[outFace_data].end(); v++) 
      {
        rate_sum += *v;
      }
      feedbackRateTag.SetRate(rate_sum);
      payloadCopy -> AddPacketTag(feedbackRateTag);

      data->SetPayload (payloadCopy);

      bool ok = incoming.m_face->SendData (data);

      DidSendOutData (inFace, incoming.m_face, data, pitEntry);

      data->SetPayload(payloadOriginal);

      // NS_LOG_DEBUG ("Satisfy " << *incoming.m_face);

      if (!ok)
      {
        m_dropData (data, incoming.m_face);
        NS_LOG_DEBUG ("Cannot satisfy data to " << *incoming.m_face);
      }
    }
  }
  // All incoming interests are satisfied. Remove them
  pitEntry->ClearIncoming ();

  // Remove all outgoing faces
  pitEntry->ClearOutgoing ();

  // Set pruning timout on PIT entry (instead of deleting the record)
  m_pit->MarkErased (pitEntry);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void
ForwardingStrategy::SatisfyPendingInterestDTCC (Ptr<Face> inFace,
                                                Ptr<Data> data,
                                                Ptr<pit::Entry> pitEntry)
{
  if (inFace == 0)
  {
    const pit::Entry::in_iterator incoming = pitEntry->GetIncoming().begin();
    uint32_t outFace_data = incoming->m_face->GetId();
    uint32_t nodeID = incoming->m_face->GetNode()->GetId();
    uint32_t inPitsize=0;

    Ptr<pit::Entry> pitEntry2 = m_pit -> Begin(); //PITエントリの最初
    while(pitEntry2) //PITエントリを全部見る
    {
      std::set<ndn::pit::IncomingFace> incoming_face  = pitEntry2 -> GetIncoming(); //incomingFaceをみる（インタレストが入ってきたFace）
      for(std::set<ndn::pit::IncomingFace>::iterator in_itr = incoming_face.begin(); in_itr != incoming_face.end(); ++in_itr)
      {
        uint32_t tmpInfaceId;
        tmpInfaceId = in_itr -> m_face -> GetId();
        if(tmpInfaceId == outFace_data) // このデータが出て行くFaceに関するPITサイズの計算
        {
          inPitsize++;
        }
      }
      pitEntry2 = m_pit -> Next(pitEntry2);
    }  //while() close//

    FwFeedbackPitsizeTag feedbackPitsizeTag;
    FwFeedbackRateTag feedbackRateTag;

    bool pitsizeTagPresent = data->GetPayload()->PeekPacketTag(feedbackPitsizeTag);
    bool rateTagPresent = data->GetPayload()->PeekPacketTag(feedbackRateTag);

    Ptr<Packet> payloadCopy = data->GetPayload()->Copy();

    uint32_t seq = data->GetName ().get (-1).toSeqNum ();
    NS_LOG_DEBUG("Node: " << nodeID << " Interest-in-face: " << outFace_data 
      << " Interest-out-face: Cache" << " b_pitsize: " << incoming->m_face->GetBPitsize()
      << " pitsize_in: " << inPitsize << " pitsize_out: 1" 
      << " f_pitsize: 1" << " f_pitsize_portion: 1" << " rateLimit: NA" 
      << " seq#: " << seq << " f_rate: NA");

    if (pitsizeTagPresent)
    {
      payloadCopy->RemovePacketTag(feedbackPitsizeTag);
      payloadCopy->RemovePacketTag(feedbackRateTag);
    }
      
    feedbackPitsizeTag.SetPitSize(inPitsize);
    payloadCopy -> AddPacketTag(feedbackPitsizeTag);
    double rate = incoming->m_face->GetObject<Limits>()->GetCurrentLimit();
    feedbackRateTag.SetRate(rate);
    payloadCopy -> AddPacketTag(feedbackRateTag);

    data->SetPayload (payloadCopy);

    bool ok = incoming->m_face->SendData (data);

    DidSendOutData (inFace, incoming->m_face, data, pitEntry);

    if (!ok)
    {
      m_dropData (data, incoming->m_face);
      NS_LOG_INFO ("Cannot satisfy data to " << *incoming->m_face);
    }
  }
  else
  {
    pitEntry->RemoveIncoming (inFace);
	  	
    Ptr<Node> node = inFace -> GetNode();
    uint32_t nodeID = node -> GetId();
    uint32_t infaceId = inFace -> GetId(); //データが入ってきたFaceのID

    //satisfy all pending incoming Interests
    BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
    {
      ///2018/1/4/////////making feedback////////////////
      const int max_number_of_face = 5; //ノードのとりうるFaceの最大数  

      uint32_t outFace_data = incoming.m_face -> GetId(); // このデータが出て行くfaceID 

      uint32_t inPitsize=0; //このデータが出て行くFaceのピットのサイズ。
      uint32_t outPitsize = 0; //このデータに関連する出て行くピットサイズ。
      uint32_t totalOutPitsize = 0; //Total outgoing pit size for the face this data arrived

      bool ExistOutFace_data=false; //このデータが出て行くFaceに関連するPITエントリかどうかを調べるフラグ。

      Ptr<pit::Entry> pitEntry2 = m_pit -> Begin(); //PITエントリの最初

      while(pitEntry2) //PITエントリを全部見る
      {
        std::set<ndn::pit::IncomingFace> incoming_face  = pitEntry2 -> GetIncoming(); //incomingFaceをみる（インタレストが入ってきたFace）
        for(std::set<ndn::pit::IncomingFace>::iterator in_itr = incoming_face.begin(); in_itr != incoming_face.end(); ++in_itr)
        {
          uint32_t tmpInfaceId;
          tmpInfaceId = in_itr -> m_face -> GetId();
          if(tmpInfaceId == outFace_data) // このデータが出て行くFaceに関するPITサイズの計算
          {
            inPitsize++;
            ExistOutFace_data = true;
          }
        }

        std::set<ndn::pit::OutgoingFace> outgoing_face  = pitEntry2 -> GetOutgoing(); //outgoingFaceをみる（インタレストが出て行ったFace）
        for(std::set<ndn::pit::OutgoingFace>::iterator out_itr = outgoing_face.begin(); out_itr != outgoing_face.end(); ++out_itr)
        {
          uint32_t tmpOutfaceId;
          tmpOutfaceId = out_itr -> m_face -> GetId();
          if(tmpOutfaceId == infaceId)
          {		
            if(ExistOutFace_data) 
            {
              outPitsize++;
            }
            totalOutPitsize++;
          }
        }

        pitEntry2 = m_pit -> Next(pitEntry2);
        ExistOutFace_data =false;
      }  //while() close//

      double pitsize_in = inPitsize;
      double pitsize_out = outPitsize;

      /* create a new FeedbackTag */
      FwFeedbackPitsizeTag feedbackPitsizeTag;
      FwFeedbackRateTag feedbackRateTag;

      bool pitsizeTagPresent = data -> GetPayload() ->  PeekPacketTag(feedbackPitsizeTag);
      bool rateTagPresent = data -> GetPayload() ->  PeekPacketTag(feedbackRateTag);

      double f_pitsize = feedbackPitsizeTag.GetPitSize();
      double f_rate = feedbackRateTag.GetRate();

      inFace->SetFPitsize(f_pitsize);
      //double f_pitsizedif = pitsize_out - f_pitsize;
      double f_pitsize_portion = f_pitsize * pitsize_out / totalOutPitsize;
      double f_pitsizedif = pitsize_out - f_pitsize_portion;
      double b_pitsize = incoming.m_face->GetBPitsize();
      double b_pitsizedif = b_pitsize - pitsize_in;
      double rate;
      double newRate;

      double tm = Simulator::Now ().ToDouble (Time::S);
      Ptr<Packet> payloadOriginal = data->GetPayload()->Copy();
      Ptr<Packet> payloadCopy = payloadOriginal->Copy();
           
      if(!pitsizeTagPresent)// for ndn-simple-dumbbell-8nodes-1bottleneck.cc
      {
        rate = (incoming.m_face->GetObject<Limits>())->GetCurrentLimit();
        newRate = rate;
        m_interestRateTable[outFace_data][infaceId] = rate;
        uint32_t seq = data->GetName().get(-1).toSeqNum();
        NS_LOG_DEBUG("Node: " << nodeID << " Interest-in-face: " << outFace_data 
          << " Interest-out-face: " << infaceId << " b_pitsize: " << b_pitsize 
          << " pitsize_in: " << pitsize_in << " pitsize_out: " << pitsize_out
          << " f_pitsize: " << f_pitsize << " f_pitsize_portion: " << f_pitsize_portion
          << " rateLimit: " << rate << " seq#: " << seq << " f_rate: NA");
      } 
      else
      {
        Ptr<Limits> faceLimits = inFace -> GetObject<Limits>();
        rate = faceLimits -> GetCurrentLimit();
                                
        double d = 0.12; //0.1 * 1500 * 8 / 10000;
        double oldRate = rate;
        double alpha = 0.2;
        if(m_consumerNeighbour)
        {
          newRate = f_rate - d * f_pitsizedif;
        }
        else
        {
          newRate = f_rate + d * (b_pitsizedif - f_pitsizedif);
        }
        if (newRate < 1) newRate = 1;
        if (newRate > faceLimits->GetMaxRate()) newRate = faceLimits->GetMaxRate();
        rate = (1 - alpha) * oldRate + alpha * newRate;
        if (rate < 1) rate = 1;
        faceLimits -> UpdateCurrentLimit(rate);
        rate = faceLimits -> GetCurrentLimit();
        uint32_t seq = data->GetName().get(-1).toSeqNum();
        NS_LOG_DEBUG("Node: " << nodeID << " Interest-in-face: " << outFace_data 
          << " Interest-out-face: " << infaceId << " b_pitsize: " << b_pitsize 
          << " pitsize_in: " << pitsize_in << " pitsize_out: " << pitsize_out
          << " f_pitsize: " << f_pitsize << " f_pitsize_portion: " << f_pitsize_portion
          << " rateLimit: " << rate << " seq#: " << seq
          << " f_rate: " << f_rate);
         
        m_interestRateTable[outFace_data][infaceId] = rate;

        payloadCopy->RemovePacketTag(feedbackPitsizeTag);
        payloadCopy->RemovePacketTag(feedbackRateTag);

      } //if(nodeID != 19  && nodeID != 20) close

      /* set pitsize on feedback */
      feedbackPitsizeTag.SetPitSize(inPitsize);
      payloadCopy -> AddPacketTag(feedbackPitsizeTag);

      /* set rate on feedback */
      double rate_sum = 0.0;
      for (std::vector<double>::iterator v = m_interestRateTable[outFace_data].begin(); v != m_interestRateTable[outFace_data].end(); v++) 
      {
        rate_sum += *v;
      }
      // feedbackRateTag.SetRate(rate_sum);
      feedbackRateTag.SetRate(rate);
      payloadCopy -> AddPacketTag(feedbackRateTag);

      data->SetPayload (payloadCopy);

      bool ok = incoming.m_face->SendData (data);

      DidSendOutData (inFace, incoming.m_face, data, pitEntry);

      data->SetPayload(payloadOriginal);

      // NS_LOG_DEBUG ("Satisfy " << *incoming.m_face);

      if (!ok)
      {
        m_dropData (data, incoming.m_face);
        NS_LOG_INFO ("Cannot satisfy data to " << *incoming.m_face);
      }
   
    } //if (inFace != 0) close
  }
  // All incoming interests are satisfied. Remove them
  pitEntry->ClearIncoming ();

  // Remove all outgoing faces
  pitEntry->ClearOutgoing ();

  // Set pruning timout on PIT entry (instead of deleting the record)
  m_pit->MarkErased (pitEntry);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
ForwardingStrategy::SatisfyPendingInterestQSF (Ptr<Face> inFace,
                                                Ptr<Data> data,
                                                Ptr<pit::Entry> pitEntry)
{
  if (inFace == 0)
  {
    const pit::Entry::in_iterator incoming = pitEntry->GetIncoming ().begin();
    uint32_t outFace_data = incoming->m_face->GetId();
    uint32_t nodeID = incoming->m_face->GetNode()->GetId();
    uint32_t inPitsize=0;

    Ptr<pit::Entry> pitEntry2 = m_pit -> Begin(); //PITエントリの最初
    while(pitEntry2) //PITエントリを全部見る
    {
      std::set<ndn::pit::IncomingFace> incoming_face  = pitEntry2 -> GetIncoming(); //incomingFaceをみる（インタレストが入ってきたFace）
      for(std::set<ndn::pit::IncomingFace>::iterator in_itr = incoming_face.begin(); in_itr != incoming_face.end(); ++in_itr)
      {
        uint32_t tmpInfaceId;
        tmpInfaceId = in_itr -> m_face -> GetId();
        if(tmpInfaceId == outFace_data) // このデータが出て行くFaceに関するPITサイズの計算
        {
          inPitsize++;
        }
      }
      pitEntry2 = m_pit -> Next(pitEntry2);
    }  //while() close//

    FwFeedbackPitsizeTag feedbackPitsizeTag;
    FwFeedbackRateTag feedbackRateTag;

    bool pitsizeTagPresent = data->GetPayload()->PeekPacketTag(feedbackPitsizeTag);
    bool rateTagPresent = data->GetPayload()->PeekPacketTag(feedbackRateTag);

    Ptr<Packet> payloadCopy = data->GetPayload()->Copy();

    uint32_t seq = data->GetName().get(-1).toSeqNum();
    NS_LOG_DEBUG("Node: " << nodeID << " Interest-in-face: " << outFace_data 
      << " Interest-out-face: Cache" << " b_pitsize: " << incoming->m_face->GetBPitsize()
      << " pitsize_in: " << inPitsize << " pitsize_out: 1" 
      << " f_pitsize: 1" << " f_pitsize_portion: 1" << " rateLimit: NA" 
      << " seq#: " << seq << " f_rate: NA");

    if (pitsizeTagPresent)
    {
      payloadCopy->RemovePacketTag(feedbackPitsizeTag);
      payloadCopy->RemovePacketTag(feedbackRateTag);
    }
      
    feedbackPitsizeTag.SetPitSize(inPitsize);
    payloadCopy -> AddPacketTag(feedbackPitsizeTag);
    double rate = incoming->m_face->GetObject<Limits>()->GetCurrentLimit();
    feedbackRateTag.SetRate(rate);
    payloadCopy -> AddPacketTag(feedbackRateTag);

    data->SetPayload (payloadCopy);

    bool ok = incoming->m_face->SendData (data);

    DidSendOutData (inFace, incoming->m_face, data, pitEntry);

    if (!ok)
    {
        m_dropData (data, incoming->m_face);
        NS_LOG_INFO ("Cannot satisfy data to " << *incoming->m_face);
    }
  }
  else
  {
    pitEntry->RemoveIncoming (inFace);
	  	
    Ptr<Node> node = inFace -> GetNode();
    uint32_t nodeID = node -> GetId();
    uint32_t infaceId = inFace -> GetId(); //データが入ってきたFaceのID

    //satisfy all pending incoming Interests
    BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
    {
    ///2018/1/4/////////making feedback////////////////
      const int max_number_of_face = 5; //ノードのとりうるFaceの最大数  

      uint32_t outFace_data = incoming.m_face -> GetId(); // このデータが出て行くfaceID 
      uint32_t inPitsize=0; //このデータが出て行くFaceのピットのサイズ。重み付けの分母。
      uint32_t outPitsize = 0;
      uint32_t totalOutPitsize = 0;

      bool ExistOutFace_data=false; //このデータが出て行くFaceに関連するPITエントリかどうかを調べるフラグ。
          //std::cout << infaceId << ",";

      Ptr<pit::Entry> pitEntry2 = m_pit -> Begin(); //PITエントリの最初

      while(pitEntry2) //PITエントリを全部見る
      {
        std::set<ndn::pit::IncomingFace> incoming_face  = pitEntry2 -> GetIncoming(); //incomingFaceをみる（インタレストが入ってきたFace）
        for(std::set<ndn::pit::IncomingFace>::iterator in_itr = incoming_face.begin(); in_itr != incoming_face.end(); ++in_itr)
        {
          uint32_t tmpInfaceId;
          tmpInfaceId = in_itr -> m_face -> GetId();
          if(tmpInfaceId == outFace_data) // このデータが出て行くFaceに関するPITサイズの計算
          {
            inPitsize++;
            ExistOutFace_data = true;
          }
        }

        std::set<ndn::pit::OutgoingFace> outgoing_face  = pitEntry2 -> GetOutgoing(); //outgoingFaceをみる（インタレストが出て行ったFace）
        for(std::set<ndn::pit::OutgoingFace>::iterator out_itr = outgoing_face.begin(); out_itr != outgoing_face.end(); ++out_itr)
        {
          uint32_t tmpOutfaceId;
          tmpOutfaceId = out_itr -> m_face -> GetId();
          if(tmpOutfaceId ==infaceId) //今回のデータが出て行くFaceに関するフィードバック情報を計算する
          {		
            if(ExistOutFace_data)
            {
              outPitsize++;
            }
            totalOutPitsize++;
          }
        }

        pitEntry2 = m_pit -> Next(pitEntry2);
        ExistOutFace_data =false;
      }  //while() close//

      double pitsize_in = inPitsize;
      double pitsize_out = outPitsize;

      /* create a new FeedbackTag */ 
      FwFeedbackPitsizeTag feedbackPitsizeTag;
      FwFeedbackRateTag feedbackRateTag;

      bool pitsizeTagPresent = data -> GetPayload() ->  PeekPacketTag(feedbackPitsizeTag);
      bool rateTagPresent = data -> GetPayload() ->  PeekPacketTag(feedbackRateTag);

      double f_pitsize = feedbackPitsizeTag.GetPitSize();
      double f_rate = feedbackRateTag.GetRate();

      inFace->SetFPitsize(f_pitsize);
      //double f_pitsizedif = pitsize_out - f_pitsize;
      double f_pitsize_portion = f_pitsize * pitsize_out / totalOutPitsize;
      double f_pitsizedif = pitsize_out - f_pitsize_portion;
      double b_pitsize = incoming.m_face->GetBPitsize();
      double b_pitsizedif = b_pitsize - pitsize_in;
      double rate;
      double newRate;

      double tm = Simulator::Now ().ToDouble (Time::S);
      Ptr<Packet> payloadOriginal = data->GetPayload()->Copy();
      Ptr<Packet> payloadCopy = payloadOriginal->Copy();           

      if (!pitsizeTagPresent) //for ndn-qsf.cc
      {
        rate = (incoming.m_face->GetObject<Limits>())->GetCurrentLimit();
        newRate = rate;
        uint32_t seq = data->GetName().get(-1).toSeqNum();
        double bw = inFace->GetBW();
        NS_LOG_DEBUG("Node: " << nodeID << " Interest-in-face: " << outFace_data 
          << " Interest-out-face: " << infaceId << " b_pitsize: " << b_pitsize 
          << " pitsize_in: " << pitsize_in << " pitsize_out: " << pitsize_out
          << " f_pitsize: " << f_pitsize << " f_pitsize_portion: " << f_pitsize_portion
          << " rateLimit: " << rate << " seq#: " << seq
          << " f_rate: " << f_rate << " bandwidth: " << bw);
        m_interestRateTable[outFace_data][infaceId] = rate;
      }  
      else               
      {
        Ptr<Limits> faceLimits = inFace -> GetObject<Limits>();
        rate = faceLimits -> GetCurrentLimit();

        double queueSize = f_pitsizedif;
        double bw = inFace->GetBW();
        if (bw > faceLimits->GetMaxRate()) bw = faceLimits->GetMaxRate();
        if (queueSize > 10.0)
          newRate = bw * 0.8;
        else
          newRate = bw;
        double oldRate = rate;
        double alpha = 0.2;
        if (newRate < 1) newRate = 1;
        if (queueSize < 5.0)
          newRate = newRate * 1.05;
        if (newRate > faceLimits->GetMaxRate()) newRate = faceLimits->GetMaxRate();
        rate = (1 - alpha) * oldRate + alpha * newRate;
        if (rate < 1) rate = 1;
        faceLimits -> UpdateCurrentLimit(rate);
        rate = faceLimits -> GetCurrentLimit();
        uint32_t seq = data->GetName ().get (-1).toSeqNum ();
        NS_LOG_DEBUG("Node: " << nodeID << " Interest-in-face: " << outFace_data 
          << " Interest-out-face: " << infaceId << " b_pitsize: " << b_pitsize 
          << " pitsize_in: " << pitsize_in << " pitsize_out: " << pitsize_out
          << " f_pitsize: " << f_pitsize << " f_pitsize_portion: " << f_pitsize_portion 
          << " rateLimit: " << rate << " seq#: " << seq
          << " f_rate: " << f_rate << " bandwidth: " << bw);
         
        m_interestRateTable[outFace_data][infaceId] = rate;

        payloadCopy->RemovePacketTag(feedbackPitsizeTag);
        payloadCopy->RemovePacketTag(feedbackRateTag);
      }

      /* set pitsize on feedback */ 
      feedbackPitsizeTag.SetPitSize(inPitsize);
      payloadCopy -> AddPacketTag(feedbackPitsizeTag);

      double rate_sum = 0.0;
      for (std::vector<double>::iterator v = m_interestRateTable[outFace_data].begin(); v != m_interestRateTable[outFace_data].end(); v++) 
      {
        rate_sum += *v;
      }
      feedbackRateTag.SetRate(rate_sum);
      payloadCopy -> AddPacketTag(feedbackRateTag);

      data->SetPayload (payloadCopy);

      bool ok = incoming.m_face->SendData (data);

      DidSendOutData (inFace, incoming.m_face, data, pitEntry);

      data->SetPayload(payloadOriginal);

      // NS_LOG_DEBUG ("Satisfy " << *incoming.m_face);

      if (!ok)
      {
        m_dropData (data, incoming.m_face);
        NS_LOG_INFO ("Cannot satisfy data to " << *incoming.m_face);
      }
   
    } //if (inFace != 0) close
  }
  // All incoming interests are satisfied. Remove them
  pitEntry->ClearIncoming ();

  // Remove all outgoing faces
  pitEntry->ClearOutgoing ();

  // Set pruning timout on PIT entry (instead of deleting the record)
  m_pit->MarkErased (pitEntry);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
ForwardingStrategy::DidReceiveSolicitedData (Ptr<Face> inFace,
                                             Ptr<const Data> data,
                                             bool didCreateCacheEntry)
{
  // do nothing
}

void
ForwardingStrategy::DidReceiveUnsolicitedData (Ptr<Face> inFace,
                                               Ptr<const Data> data,
                                               bool didCreateCacheEntry)
{
  // do nothing
}

void
ForwardingStrategy::WillSatisfyPendingInterest (Ptr<Face> inFace,
                                                Ptr<pit::Entry> pitEntry)
{
  pit::Entry::out_iterator out = pitEntry->GetOutgoing ().find (inFace);

  // If we have sent interest for this data via this face, then update stats.
  if (out != pitEntry->GetOutgoing ().end ())
    {
      pitEntry->GetFibEntry ()->UpdateFaceRtt (inFace, Simulator::Now () - out->m_sendTime);
    }

  m_satisfiedInterests (pitEntry);
}

bool
ForwardingStrategy::ShouldSuppressIncomingInterest (Ptr<Face> inFace,
                                                    Ptr<const Interest> interest,
                                                    Ptr<pit::Entry> pitEntry)
{
  bool isNew = pitEntry->GetIncoming ().size () == 0 && pitEntry->GetOutgoing ().size () == 0;

  if (isNew) return false; // never suppress new interests

  bool isRetransmitted = m_detectRetransmissions && // a small guard
                         DetectRetransmittedInterest (inFace, interest, pitEntry);

  if (pitEntry->GetOutgoing ().find (inFace) != pitEntry->GetOutgoing ().end ())
    {
      NS_LOG_DEBUG ("Non duplicate interests from the face we have sent interest to. Don't suppress");
      // got a non-duplicate interest from the face we have sent interest to
      // Probably, there is no point in waiting data from that face... Not sure yet

      // If we're expecting data from the interface we got the interest from ("producer" asks us for "his own" data)
      // Mark interface YELLOW, but keep a small hope that data will come eventually.

      // ?? not sure if we need to do that ?? ...

      // pitEntry->GetFibEntry ()->UpdateStatus (inFace, fib::FaceMetric::NDN_FIB_YELLOW);
    }
  else
    if (!isNew && !isRetransmitted)
      {
        return true;
      }

  return false;
}

void
ForwardingStrategy::PropagateInterest (Ptr<Face> inFace,
                                       Ptr<Interest> interest,
                                       Ptr<pit::Entry> pitEntry)
{
  bool isRetransmitted = m_detectRetransmissions && // a small guard
                         DetectRetransmittedInterest (inFace, interest, pitEntry);

  pitEntry->AddIncoming (inFace/*, interest->GetInterestLifetime ()*/);
  /// @todo Make lifetime per incoming interface
  pitEntry->UpdateLifetime (interest->GetInterestLifetime ());

  bool propagated = DoPropagateInterest (inFace, interest, pitEntry);

  if (!propagated && isRetransmitted) //give another chance if retransmitted
    {
      // increase max number of allowed retransmissions
      pitEntry->IncreaseAllowedRetxCount ();
      
      // try again
      propagated = DoPropagateInterest (inFace, interest, pitEntry);
    }

  // if (!propagated)
  //   {
  //     NS_LOG_DEBUG ("++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  //     NS_LOG_DEBUG ("+++ Not propagated ["<< interest->GetName () <<"], but number of outgoing faces: " << pitEntry->GetOutgoing ().size ());
  //     NS_LOG_DEBUG ("++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  //   }

  // ForwardingStrategy will try its best to forward packet to at least one interface.
  // If no interests was propagated, then there is not other option for forwarding or
  // ForwardingStrategy failed to find it.
  if (!propagated && pitEntry->AreAllOutgoingInVain ())
    {
      DidExhaustForwardingOptions (inFace, interest, pitEntry);
    }
}

bool
ForwardingStrategy::CanSendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<Interest> interest,
                                        Ptr<pit::Entry> pitEntry)
{
  if (outFace == inFace)
    {
      // NS_LOG_DEBUG ("Same as incoming");
      return false; // same face as incoming, don't forward
    }

  pit::Entry::out_iterator outgoing =
    pitEntry->GetOutgoing ().find (outFace);
// NS_LOG_DEBUG(pitEntry->GetOutgoing().size());
  if (outgoing != pitEntry->GetOutgoing ().end ())
    {
      
      if (!m_detectRetransmissions)
        return false; // suppress
      else if (outgoing->m_retxCount >= pitEntry->GetMaxRetxCount ())
        {
          // NS_LOG_DEBUG ("Already forwarded before during this retransmission cycle (" <<outgoing->m_retxCount << " >= " << pitEntry->GetMaxRetxCount () << ")");
          return false; // already forwarded before during this retransmission cycle
        }
   }

  return true;
}

bool
ForwardingStrategy::CanSendOutInterestFromQ (Ptr<Limits> limits)
{
  NS_LOG_DEBUG(this);
}

bool
ForwardingStrategy::TrySendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<Interest> interest,
                                        Ptr<pit::Entry> pitEntry)
{
  double tm = Simulator::Now().ToDouble(Time::S);

  uint32_t nodeID = inFace->GetNode() -> GetId();
  uint32_t seq = interest->GetName ().get (-1).toSeqNum ();
  Ptr<Limits> faceLimits = outFace -> GetObject<Limits>();
  // Ptr<DelayedInterest> di = Create<DelayedInterest>();
  // di->m_inFace = inFace;
  // di->m_outFace = outFace;
  // di->m_interest = interest;
  // di->m_pitEntry = pitEntry;
  // di->m_fs = this;
  // faceLimits->Enqueue(di);
  // NS_LOG_LOGIC("Enqueue: Node: " << nodeID 
  //          << " interfaceID: " << inFace -> GetId() 
  //          << " seq#: " << seq);
  if (!CanSendOutInterest (inFace, outFace, interest, pitEntry))
    {
      return true;
    }
  // di = faceLimits->Dequeue();
  // inFace = di->m_inFace;
  // outFace = di->m_outFace;
  // interest = di->m_interest;
  // pitEntry = di->m_pitEntry;
  // nodeID = inFace->GetNode() -> GetId();
  // seq = interest->GetName ().get (-1).toSeqNum ();
  // NS_LOG_LOGIC("Dequeue: Node: " << nodeID 
  //          << " interfaceID: " << inFace -> GetId() 
  //          << " seq#: " << seq);
  /*
  Ptr<Node> node = inFace -> GetNode();
  //uint32_t nodeID = node -> GetId();
  faceLimits = outFace -> GetObject<Limits>();
  double rate = faceLimits -> GetCurrentLimit();
  uint32_t faceid = outFace->GetId();

  pitEntry->AddOutgoing (outFace);  

  Ptr<pit::Entry> pe = m_pit->Begin();
  double pc = 0;
  while(pe)
  {
    std::set<ndn::pit::OutgoingFace> outgoing_face = pe->GetOutgoing();
    for(std::set<ndn::pit::OutgoingFace>::iterator out_itr = outgoing_face.begin();
      out_itr != outgoing_face.end(); ++out_itr)
      {
        if(out_itr->m_face->GetId() == faceid)
        {
          pc++;
        }
      }
    pe = m_pit->Next(pe);
  }
  
  FwFeedbackPitsizeTag feedbackPitsizeTag;
  feedbackPitsizeTag.SetPitSize(pc);

  Ptr<Packet> payload = interest->GetPayload()->Copy();
  payload->ReplacePacketTag(feedbackPitsizeTag);
  const Interest* imutableInterest = &(*interest);
  Interest* mutableInterest = const_cast<Interest*>(imutableInterest);
  mutableInterest->SetPayload(payload);
  
  NS_LOG_LOGIC("Node: " << nodeID
              << " interfaceID: " << faceid
               << " pitsize: " << m_pit->GetSize()
              << " rate: " << rate << " seq#: " << seq);

//  pitEntry->AddOutgoing (outFace);

  //transmission
  bool successSend = outFace->SendInterest (interest);
  if (!successSend)
    {
      m_dropInterests (interest, outFace);
    }

  DidSendOutInterest (inFace, outFace, interest, pitEntry);
*/
  return true;
}

bool
ForwardingStrategy::RetrySendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<Interest> interest,
                                        Ptr<pit::Entry> pitEntry)
{
  double tm = Simulator::Now().ToDouble(Time::S);

  uint32_t seq = interest->GetName ().get (-1).toSeqNum ();
  Ptr<Node> node = inFace -> GetNode();
  uint32_t nodeID = node -> GetId();
  Ptr<Limits> faceLimits = outFace -> GetObject<Limits>();
  double rate = faceLimits -> GetCurrentLimit();
  uint32_t faceid = outFace->GetId();

  pitEntry->AddOutgoing (outFace);  

  Ptr<pit::Entry> pe = m_pit->Begin();
  double pc = 0;
  while(pe)
  {
    std::set<ndn::pit::OutgoingFace> outgoing_face = pe->GetOutgoing();
    for(std::set<ndn::pit::OutgoingFace>::iterator out_itr = outgoing_face.begin();
      out_itr != outgoing_face.end(); ++out_itr)
      {
        if(out_itr->m_face->GetId() == faceid)
        {
          pc++;
        }
      }
    pe = m_pit->Next(pe);
  }
  
  FwFeedbackPitsizeTag feedbackPitsizeTag;
  feedbackPitsizeTag.SetPitSize(pc);

  Ptr<Packet> payload = interest->GetPayload()->Copy();
  payload->ReplacePacketTag(feedbackPitsizeTag);
  const Interest* imutableInterest = &(*interest);
  Interest* mutableInterest = const_cast<Interest*>(imutableInterest);
  mutableInterest->SetPayload(payload);

  NS_LOG_LOGIC("Node: " << nodeID
              << " interfaceID: " << faceid
               << " pitsize: " << m_pit->GetSize()
              << " rate: " << rate << " seq#: " << seq);

//  pitEntry->AddOutgoing (outFace);

  //transmission
  bool successSend = outFace->SendInterest (interest);
  if (!successSend)
    {
      m_dropInterests (interest, outFace);
    }

  DidSendOutInterest (inFace, outFace, interest, pitEntry);

  return true;
}

void
ForwardingStrategy::DidSendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<const Interest> interest,
                                        Ptr<pit::Entry> pitEntry)
{
  m_outInterests (interest, outFace);
}

void
ForwardingStrategy::DidSendOutData (Ptr<Face> inFace,
                                    Ptr<Face> outFace,
                                    Ptr<const Data> data,
                                    Ptr<pit::Entry> pitEntry)
{
  m_outData (data, inFace == 0, outFace);
  
}

void
ForwardingStrategy::WillEraseTimedOutPendingInterest (Ptr<pit::Entry> pitEntry)
{
  m_timedOutInterests (pitEntry);
}

uint32_t
ForwardingStrategy::NPitEntryWithOutgoingFace (Ptr<Face> outFace)
{
  uint32_t faceid = outFace->GetId();

  Ptr<pit::Entry> pe = m_pit->Begin();
  uint32_t pc = 0;
  while(pe)
  {
    std::set<ndn::pit::OutgoingFace> outgoing_face = pe->GetOutgoing();
    for(std::set<ndn::pit::OutgoingFace>::iterator out_itr = outgoing_face.begin();
      out_itr != outgoing_face.end(); ++out_itr)
      {
        if(out_itr->m_face->GetId() == faceid)
        {
          pc++;
        }
      }
    pe = m_pit->Next(pe);
  }
  return pc;
}

void
ForwardingStrategy::AddFace (Ptr<Face> face)
{
  // do nothing here
  // std::vector<double> c({0.0});
  m_interestRateTable.push_back(std::vector<double> ({0.0}));
  std::size_t n = m_interestRateTable.size();
  for (std::vector< std::vector<double> >::iterator p = m_interestRateTable.begin(); p != m_interestRateTable.end(); p++) {
    for (size_t i = p->size(); i < n; i++ ) {
      p->push_back(0.0);
    }
    // std::cout << "column size:" << p->size() << " ";
  }
  // std::cout << "row size:" << n << std::endl;
}

void
ForwardingStrategy::RemoveFace (Ptr<Face> face)
{
  // do nothing here
}

void
ForwardingStrategy::DidAddFibEntry (Ptr<fib::Entry> fibEntry)
{
  // do nothing here
}

void
ForwardingStrategy::WillRemoveFibEntry (Ptr<fib::Entry> fibEntry)
{
  // do nothing here
}


} // namespace ndn
} // namespace ns3
