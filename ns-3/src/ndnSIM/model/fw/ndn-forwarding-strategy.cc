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

namespace ll = boost::lambda;

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (ForwardingStrategy);

NS_LOG_COMPONENT_DEFINE (ForwardingStrategy::GetLogName ().c_str ());

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
    ;
  return tid;
}
///////////////////////////////////////////////
ForwardingStrategy::ForwardingStrategy ()
{
  ad = 5.0;
  DataPacketNum[20] = {};//LEE
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

  Ptr<Node> node = inFace -> GetNode();
  uint32_t nodeID = node -> GetId();
  Ptr<Limits> faceLimits = inFace -> GetObject<Limits>();
  double rate = faceLimits -> GetCurrentLimit();
  uint32_t faceid = inFace->GetId();
  //  if ((nodeID == 10 && faceid == 3) || nodeID == 14 || nodeID == 15){
  std::cout << Simulator::Now ().ToDouble (Time::S) << " " //time
            << "Node:" << nodeID << " "
            << "interfaceID:" << inFace -> GetId() << " "  // incomingDataFaceID
            // << m_pit->GetSize() << "\t"
            << "rate:" << rate << "\n";
  //          << "comeInterest" << "\n";
  //  }
  
//////////////////////////////////////////////////////////////////
  
  FwFeedbackPitsizeTag bwFeedbackPitsizeTag;
  if (interest->GetPayload()->PeekPacketTag(bwFeedbackPitsizeTag))
  {
    double b_pitsize = bwFeedbackPitsizeTag.GetPitSize();
    inFace->SetBPitsize(b_pitsize);
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
                SatisfyPendingInterest (0, contentObject, pitEntry);
                return;
        }

        if (similarInterest && ShouldSuppressIncomingInterest (inFace, interest, pitEntry))
        {
                pitEntry->AddIncoming (inFace/*, interest->GetInterestLifetime ()*/);
                // update PIT entry lifetime
                pitEntry->UpdateLifetime (interest->GetInterestLifetime ());

                // Suppress this interest if we're still expecting data from some other face
                NS_LOG_DEBUG ("Suppress interests");
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
/*  Ptr<Node> node = inFace -> GetNode();
  uint32_t nodeID = node -> GetId();
  Ptr<Limits> faceLimits = inFace -> GetObject<Limits>();
  double rate = faceLimits -> GetCurrentLimit();
  
  std::cout << Simulator::Now ().ToDouble (Time::S) << "\t" //time
            << nodeID << "\t"
            //<< inFace -> GetId() << "\t"  // incomingDataFaceID
            //<< m_pit->GetSize() << "\t"
            << rate << "\t"
            << "comeData" << "\n";
    */
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
 

                if (feedbackPitsizeTag.GetPitSize() >= 0)
                {      
      	                SatisfyPendingInterestDTCC (inFace, data, pitEntry);
                }
                else
                {
                        SatisfyPendingInterest (inFace, data, pitEntry);
                        //std::cout << "data包的Dh已经是0了，不用调整步数"<< "\n"; 
                }

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

  if (existingInFace != pitEntry->GetIncoming ().end ())
    {
      // this is almost definitely a retransmission. But should we trust the user on that?
      isRetransmitted = true;
    }

  return isRetransmitted;
}

void
ForwardingStrategy::SatisfyPendingInterest (Ptr<Face> inFace,
                                            Ptr<const Data> data,
                                            Ptr<pit::Entry> pitEntry)
{
        if (inFace != 0)
                pitEntry->RemoveIncoming (inFace);

                //satisfy all pending incoming Interests
                BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
        {
                bool ok = incoming.m_face->SendData (data);

                DidSendOutData (inFace, incoming.m_face, data, pitEntry);
                NS_LOG_DEBUG ("Satisfy " << *incoming.m_face);

                if (!ok)
                {
                        m_dropData (data, incoming.m_face);
                        NS_LOG_DEBUG ("Cannot satisfy data to " << *incoming.m_face);
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
  std::cout << "Node:" << inFace->GetNode()->GetId() << " infaceID:" << inFace->GetId() << std::endl;
  if (inFace != 0)
    pitEntry->RemoveIncoming (inFace);
  //std::cout << Simulator::Now ().ToDouble (Time::S) << "\n"; // time

  //satisfy all pending incoming Interests
  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ()){
    ///2018/1/4/////////making feedback////////////////
    const int max_number_of_face = 5; //ノードのとりうるFaceの最大数  

    uint32_t outFace_data = incoming.m_face -> GetId(); // このデータが出て行くfaceID 
    //std::cout << "this Data will go to Face No." << outFace_data << "\n";
    uint32_t inPitsize=0; //このデータが出て行くFaceのピットのサイズ。重み付けの分母。
    uint32_t inPitsize_face[max_number_of_face] = {}; //このデータに関連する出て行くピットサイズ。重み付けの分子。

    bool ExistOutFace_data=false; //このデータが出て行くFaceに関連するPITエントリかどうかを調べるフラグ。

    double outPitsize[max_number_of_face] = {}; //出て行くインタレストに関するピットのサイズ。ピットサイズ差算出に使う。各FaceのPITサイズ。

    double outPitsizedif[max_number_of_face] = {}; //出て行くinterstに関するピット(outPIT)サイズの差。重み付けされる。各Faceの待機パケット数。
    double interestRate[max_number_of_face] = {}; //このデータに関するPITに関するInterest送出Faceの送出レート。重み付けされる。各Faceの送出レート。

    Ptr<pit::Entry> pitEntry2 = m_pit -> Begin(); //PITエントリの最初

    while(pitEntry2) //PITエントリを全部見る
    {
      std::set<ndn::pit::IncomingFace> incoming_face  = pitEntry2 -> GetIncoming(); //incomingFaceをみる（インタレストが入ってきたFace）
      for(std::set<ndn::pit::IncomingFace>::iterator in_itr = incoming_face.begin(); in_itr != incoming_face.end(); ++in_itr)
      {
        uint32_t infaceId;
        infaceId = in_itr -> m_face -> GetId();
        //std::cout << infaceId << ",";
        if(infaceId == outFace_data) // このデータが出て行くFaceに関するPITサイズの計算
        {
          inPitsize++;
          ExistOutFace_data = true;
        }
      }

      // if(inPitsize>0) ExistOutFace_data = true;

      std::set<ndn::pit::OutgoingFace> outgoing_face  = pitEntry2 -> GetOutgoing(); //outgoingFaceをみる（インタレストが出て行ったFace）
      for(std::set<ndn::pit::OutgoingFace>::iterator out_itr = outgoing_face.begin(); out_itr != outgoing_face.end(); ++out_itr)
      {
        uint32_t outfaceId;
        outfaceId = out_itr -> m_face -> GetId();
        outPitsize[outfaceId] = outPitsize[outfaceId] + 1; //Face毎の出て行ったインタレストに関するPIT（OutPITと呼ぶ）のサイズを計算

        outPitsizedif[outfaceId] = out_itr -> m_face -> GetPitsizedif(); //face毎のPITサイズ差
        interestRate[outfaceId] = out_itr -> m_face -> GetObject<Limits>() -> GetCurrentLimit(); //Face毎の転送レート

        if(ExistOutFace_data) //今回のデータが出て行くFaceに関するフィードバック情報を計算する
        {		
          inPitsize_face[outfaceId] = inPitsize_face[outfaceId] + 1;
        }
      }

      pitEntry2 = m_pit -> Next(pitEntry2);
      ExistOutFace_data =false;
      // inPitsize = 0; // 20221015 inPitsize reset miss fixed.
    }  //while() close//
	  	
    Ptr<Node> node = inFace -> GetNode();
    uint32_t nodeID = node -> GetId();
    uint32_t infaceId = inFace -> GetId(); //データが入ってきたFaceのID

    double pitsize_in = inPitsize;
    double pitsize_out = outPitsize[infaceId];

    DataPacketNum[nodeID]++;//Lee

    /* create a new FeedbackTag */
    FwFeedbackPitsizeTag feedbackPitsizeTag;
    FwFeedbackRateTag feedbackRateTag;
    FwFeedbackPitsizeDifferenceTag feedbackPitsizeDifferenceTag;        

    data -> GetPayload() ->  PeekPacketTag(feedbackPitsizeTag);
    data -> GetPayload() ->  PeekPacketTag(feedbackRateTag);
    data -> GetPayload() ->  PeekPacketTag(feedbackPitsizeDifferenceTag);

    double f_pitsize = feedbackPitsizeTag.GetPitSize();
    double f_rate = feedbackRateTag.GetRate();
    double f_pitsizedif = feedbackPitsizeDifferenceTag.GetPitSizeDif();

    inFace->SetFPitsize(f_pitsize);
    f_pitsizedif = pitsize_out - f_pitsize;
    double b_pitsize = incoming.m_face->GetBPitsize();
    double b_pitsizedif = b_pitsize - pitsize_in;

    if(f_pitsizedif != 0 || f_pitsize != 0 || f_rate != 0)
    {
      //check whether tag set or no!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!///////
      //data tags maid by intermadiate router doesn't have any value./////////  

      /* get pitsize */
      double pitsize = outPitsize[infaceId]; // データが入ってきたFaceのPITサイズ ///LEE: InPIT
      double pitsizedif = pitsize - f_pitsize; // データが入ってきたFaceのPITサイズ差  ///LEE : InPIT-OutPIT
      // if (nodeID == 14){
        // NS_LOG_DEBUG ("nodeID 14: pitsize: " << pitsize );
      // }      double pitsizedif = pitsize - f_pitsize; // データが入ってきたFaceのPITサイズ差  ///LEE : InPIT-OutPIT
      // if(nodeID == 16 || nodeID == 17 || nodeID == 18 || nodeID == 19 || nodeID == 20) //20211019 for ndn-congestion-topo54src.cc
      if(nodeID == 8 || nodeID == 9 || nodeID == 10 || nodeID == 11) //20220921 for ndn-congestion-topo-dumbbell-12nodes.cc
      {
        // pitsize = 0;
        pitsizedif = 0; // Half size of uint32_t (MAX uint32_t = 4294967295 )
      } 

      /* get current interest sending rate limit */
      Ptr<Limits> faceLimits = inFace -> GetObject<Limits>();
      double rate = faceLimits -> GetCurrentLimit();
      //std::cout << "cur_rate = " << rate << "\n";
      double newRate;

      double tm = Simulator::Now ().ToDouble (Time::S);
           
      // if(nodeID == 16 || nodeID == 17 || nodeID == 18 || nodeID == 19 || nodeID == 20) //20211019 for ndn-congestion-topo54src.cc
      if(nodeID == 8 || nodeID == 9 || nodeID == 10 || nodeID == 11) //20220921 for ndn-congestion-topo-dumbbell-12nodes.cc
      {
        // rate = 100000;
        rate = (incoming.m_face->GetObject<Limits>())->GetCurrentLimit();
        newRate = rate;
        m_interestRateTable[outFace_data][infaceId] = rate;
        std::cout << "Node: " << nodeID << "\t Interest-in-face: " << outFace_data 
          << "\t Interest-out-face: " << infaceId << "\t b_pitsize: " << b_pitsize 
          << "\t pitsize_in: " << pitsize_in << "\t pitsize_out: " << pitsize_out
          << "\t f_pitsize: " << f_pitsize << "\t b_pitsizedif: " << b_pitsizedif
          << "\t f_pitsizedif: " << f_pitsizedif << "\t f_rate: " << f_rate 
          << "\t new rate: " << rate << "\t newRate: " << newRate << std::endl;
        std::cout << "\t Data#: " << DataPacketNum[nodeID] << "\t time: " << tm 
          << "\t Name: " << data->GetName() << std::endl;
        // std::cout << "m_interestRateTable size:" << m_interestRateTable.size() << std::endl;
      }  
                                 
      /* update interest sendinrg rate limit */
      // if(nodeID != 16 && nodeID != 17 && nodeID != 18 && nodeID != 19 && nodeID != 20) //20211019 for ndn-congestion-topo54src.cc
      if(nodeID != 8 && nodeID != 9 && nodeID != 10 && nodeID != 11) //20220921 for ndn-congestion-topo-dumbbell-12nodes.cc
      {
        // if(nodeID == 0 || nodeID == 1 || nodeID == 2 || nodeID == 3 || nodeID ==4)//for src 1 , src 2, src3, src4, src5//aikawa
        /* if(nodeID == 0 || nodeID == 1 || nodeID == 2 || nodeID == 3)//for src 1 , src 2, src3, src4//nakazato
        {
          rate = 100000;
          newRate = rate;
        } */ //lee
                               
//        double tm = Simulator::Now ().ToDouble (Time::S);
                                
        // double d = 0.1;                          
        double d = 0.12; //0.1 * 1500 * 8 / 10000;
        double oldRate = rate;
        double alpha = 0.2;
        if(nodeID == 0 || nodeID == 1 || nodeID == 2 || nodeID == 3)
        {
          newRate = f_rate - d * f_pitsizedif;
        }
        else
        {
          //rate = f_rate - d*(f_pitsizedif - pitsizedif); //(origin) rate計算の式        		
          // rate = f_rate - d*(f_pitsizedif - pitsizedif + DataPacketNum[nodeID]); //LEE's proposal                    
          // newRate = f_rate - d*(f_pitsizedif - pitsizedif);
          // newRate = f_rate - d*(pitsizedif - f_pitsizedif);
          newRate = f_rate + d * (b_pitsizedif - f_pitsizedif);
        }
        if (newRate < 1) newRate = 1;
        //rate = (1 - alpha) * oldRate + alpha * (f_rate - d*(f_pitsizedif - pitsizedif + DataPacketNum[nodeID]));
        rate = (1 - alpha) * oldRate + alpha * newRate;
        if (rate < 1) rate = 1;
        /*
        NS_LOG_DEBUG("Node " << nodeID << " infaceID " << infaceId << ": old rate:" << oldRate
                    << " new rate:" << rate << " f_rate:" << f_rate
                    << " pitsize:" << pitsize << " f_pitsize:" << f_pitsize 
                    << "pitsizedif:" << pitsizedif << " f_pitsizedif:" << f_pitsizedif 
                    << std::endl); */
        /* std::cout << "Before limit: Node:" << nodeID << " infaceID:" << infaceId << " old rate:" << oldRate
          << " new rate:" << rate << " newRate:" << newRate << " f_rate:" << f_rate
          << " pitsize:" << pitsize << " f_pitsize:" << f_pitsize 
          << " pitsizedif:" << pitsizedif << " f_pitsizedif:" << f_pitsizedif 
          << " Data#:" << DataPacketNum[nodeID] << std::endl;
        std::cout << "---------------------------" << std::endl; */
        std::cout << "Node: " << nodeID << "\t Interest-in-face: " << outFace_data 
          << "\t Interest-out-face: " << infaceId << "\t b_pitsize: " << b_pitsize 
          << "\t pitsize_in: " << pitsize_in << "\t pitsize_out: " << pitsize_out
          << "\t f_pitsize: " << f_pitsize << "\t b_pitsizedif: " << b_pitsizedif
          << "\t f_pitsizedif: " << f_pitsizedif << "\t f_rate: " << f_rate << "\t old rate: " << oldRate
          << "\t new rate: " << rate << "\t computed newRate: " << newRate << std::endl;
        // }
         
        // if(rate < 0) rate = 1;
        faceLimits -> UpdateCurrentLimit(rate);
        rate = faceLimits -> GetCurrentLimit();
        // if (newRate <= 0) newRate = 1;
        if (newRate > faceLimits->GetMaxRate()) newRate = faceLimits->GetMaxRate();

        m_interestRateTable[outFace_data][infaceId] = rate;
        // std::cout << "nodeID:" << nodeID << " inface:" << outFace_data << " outface:" << infaceId << " rate:" << rate << std::endl;
        // std::cout << "m_interestRateTable size:" << m_interestRateTable.size() << std::endl;

        // if((nodeID == 10 && infaceId == 7) || nodeID == 7 || nodeID == 8 || nodeID == 14){
        // if ((nodeID == 2 || nodeID == 7 || (nodeID == 10 && infaceId == 7) || nodeID == 14 || nodeID == 15)){
        // if(nodeID == 10){
        NS_LOG_DEBUG("Node " << nodeID << " infaceID " << infaceId << " | rate:" << rate
                    << " pitsizedif:" << pitsizedif << " f_pitsizedif:" << f_pitsizedif
                    << " DataPacketNum:" << DataPacketNum[nodeID] 
                    << " face-info:" << *inFace);
        /* std::cout << "After limit: " << tm << " Node:" << nodeID << " infaceID:" << infaceId << " old rate:" << oldRate
          << " new rate:" << rate << " newRate:" << newRate << " f_rate:" << f_rate
          << " pitsize:" << pitsize << " f_pitsize:" << f_pitsize 
          << " pitsizedif:" << pitsizedif << " f_pitsizedif:" << f_pitsizedif 
          << " Data#:" << DataPacketNum[nodeID] << std::endl;
        std::cout << "---------------------------" << std::endl; */
        std::cout << "new rate after limit:\t" << rate << "\t newRate after limit:\t" << newRate << "\t f_rate:\t" << f_rate
          << "\t Data#:\t" << DataPacketNum[nodeID] << "\t time:\t" << tm 
          << "\t Name:\t" << data->GetName() << "\t inPitsize\t" << inPitsize << std::endl;
        // }

        //std::cout << "!!!!!!!!!!__pitsizedif_didn't set_the value_at_neighbor_router__!!!!!!!!!!" << "\n";

        outPitsizedif[infaceId] = pitsizedif; //データの更新
        inFace->SetPitsizedif(pitsizedif);
        interestRate[infaceId] = rate;

        double f_pitsize_test=0;
        double f_pitsizedif_test=0;
        double f_rate_test=0;

        f_pitsize_test = inPitsize;
        //std::cout << "f_pitsize_test is " << f_pitsize_test << "\n";
        // pitsize= f_pitsize_test;

        //std::cout << "f_pitsize_test is " << f_pitsize_test << " and pitsize is " << pitsize << "\n"; 
        double w[max_number_of_face] ={};

        for(int i=0;i<max_number_of_face;i++)
        {
          w[i] = (double)inPitsize_face[i]/(double)inPitsize;
          //std::cout << "w[" << i << "] is " << w[i] << "\n";
        }

        double w_f_pitsizedif[max_number_of_face]={};
        for(int i=0;i<max_number_of_face;i++)
        {
          w_f_pitsizedif[i] = w[i]*outPitsizedif[i];
          //std::cout << "w_f_pitsizedif[" << i << "] is " << w_f_pitsizedif[i] << "\n";
          f_pitsizedif_test = f_pitsizedif_test + w_f_pitsizedif[i];
        }
        //std::cout << "f_pitsizedif_test is " << f_pitsizedif_test << "\n";       

        //pitsizedif = f_pitsizedif_test;

        double w_f_rate[max_number_of_face]={};
        for(int i=0;i<max_number_of_face;i++)
        {
          w_f_rate[i] = w[i] * interestRate[i];
          //std::cout << "w_f_rate[" << i << "] is " << w_f_rate[i] << "\n";
          f_rate_test = f_rate_test + w_f_rate[i];
        }
        //std::cout << "f_rate_test is " << f_rate_test << "\n";
        //rate = f_rate_test;                        
        //2019/1/switch dfcc or non_dfcc ??

      } //if(nodeID != 19  && nodeID != 20) close

      //std::cout << " upd_rate = " << rate << "\n"; 
 
      /* remove old tag? */      		
      Ptr<Packet> payloadCopy = data -> GetPayload() -> Copy();
      bool pitsizetagExist = payloadCopy -> RemovePacketTag(feedbackPitsizeTag);
      bool ratetagExist = payloadCopy -> RemovePacketTag(feedbackRateTag);
      bool pitsizediftagExist = payloadCopy -> RemovePacketTag(feedbackPitsizeDifferenceTag);

      if(pitsizetagExist && ratetagExist && pitsizediftagExist)
      { 
        /* set pitsize on feedback */
        // feedbackPitsizeTag.SetPitSize(pitsize);
        feedbackPitsizeTag.SetPitSize(inPitsize);
        payloadCopy -> AddPacketTag(feedbackPitsizeTag);
        //std::cout << "feedbackPitsizeTag.GetPitSize() = " << feedbackPitsizeTag.GetPitSize() << "\n";
        data->SetPayload (payloadCopy);

        /* set rate on feedback */
        // feedbackRateTag.SetRate(rate);
        /* std::cout << "nodeID:" << nodeID;
        std::cout << " m_interestRateTable size:" << m_interestRateTable.size() << std::endl;
        for (std::vector< std::vector<double> >::iterator vo = m_interestRateTable.begin(); vo != m_interestRateTable.end(); vo++) {
          for (std::vector<double>::iterator vi = vo->begin(); vi != vo->end(); vi++) {
            std::cout << " " << *vi;
          }
          std::cout << std::endl;
        } */

        double rate_sum = 0.0;
        for (std::vector<double>::iterator v = m_interestRateTable[outFace_data].begin(); v != m_interestRateTable[outFace_data].end(); v++) {
          rate_sum += *v;
          // std::cout << ", " << *v;
        }
        // std::cout << "sum:" << rate_sum << std::endl;
        feedbackRateTag.SetRate(rate_sum);

        // feedbackRateTag.SetRate(newRate);
        // std::cout << "rate:" << rate << ", newRate:" << newRate << std::endl;
        payloadCopy -> AddPacketTag(feedbackRateTag);
        //std::cout << "feedbackRateTag.GetRate() = " << feedbackRateTag.GetRate() << "\n";
        data->SetPayload (payloadCopy);

        // 20221015 PIT size diff to be feed back should be proportional to 
        // the number of PIT entries whose corresponding Interest packets enter the
        // router at the face where this Data packet is going out.
        pitsizedif *= (inPitsize_face[infaceId] / outPitsize[infaceId]);

        feedbackPitsizeDifferenceTag.SetPitSizeDif(pitsizedif);
        payloadCopy -> AddPacketTag(feedbackPitsizeDifferenceTag);
        //std::cout << "feedbackPitsizeDifferenceTag.GetPitDifSize() = " << feedbackPitsizeDifferenceTag.GetPitDifSize() << "\n";
        // std::cout << "feedbackPitsizeTag.GetPitSize() = " << feedbackPitsizeTag.GetPitSize() << "\n";
        data->SetPayload (payloadCopy);
      }
    } //if(f_pitsizedif != 0 || f_pitsize != 0 || f_rate != 0) close
      //2017/12/14////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    bool ok = incoming.m_face->SendData (data);

    DidSendOutData (inFace, incoming.m_face, data, pitEntry);
    DataPacketNum[nodeID]--;
    NS_LOG_DEBUG ("Satisfy " << *incoming.m_face);

    if (!ok)
    {
      m_dropData (data, incoming.m_face);
      NS_LOG_DEBUG ("Cannot satisfy data to " << *incoming.m_face);
    }
   
  } //if (inFace != 0) close

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
                                        Ptr<const Interest> interest,
                                        Ptr<pit::Entry> pitEntry)
{
  if (outFace == inFace)
    {
      // NS_LOG_DEBUG ("Same as incoming");
      return false; // same face as incoming, don't forward
    }

  pit::Entry::out_iterator outgoing =
    pitEntry->GetOutgoing ().find (outFace);
NS_LOG_DEBUG(pitEntry->GetOutgoing().size());
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
ForwardingStrategy::TrySendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<Interest> interest,
                                        Ptr<pit::Entry> pitEntry)
{
  if (!CanSendOutInterest (inFace, outFace, interest, pitEntry))
    {
      return false;
    }

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
  interest->SetPayload(payload);
  
  double tm = Simulator::Now().ToDouble(Time::S);
  // if (nodeID == 2 || nodeID == 7 || (nodeID == 10 && faceid == 7) || nodeID == 14 || nodeID == 15){
  /* if (nodeID == 2 || nodeID == 7){
    std::cout << Simulator::Now ().ToDouble (Time::S) << " " //time
              << "Node:" << nodeID << " "
              << "interfaceID:" << faceid << " "  // incomingDataFaceID
              // << m_pit->GetSize() << "\t"
              << "rate:" << rate << "\n";
    std::cout << "--------------------------------\n";
  } */

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
