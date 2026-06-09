/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2012 University of California, Los Angeles
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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */
// ndn-congestion-topo-plugin.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include <string>
#include <iomanip>
#include <iostream>
#include <time.h>
#include <functional>

using namespace ns3;

/**
 * This scenario simulates a grid topology (using topology reader module)
 
 * To run scenario and see what is happening, use the following command:
 *
 *     NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=ndn-congestion-topo-plugin-5src
 */


NS_LOG_COMPONENT_DEFINE("ndn-dumbbell-6nodes-DCQL");

int
main (int argc, char *argv[])
{
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("100"));
  //Config::SetDefault("ns3::ndn::Pit::PitEntryPruningTimeout",StringValue("9999"));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  AnnotatedTopologyReader topologyReader ("", 25);
  topologyReader.SetFileName ("src/ndnSIM/examples/topologies/topo-dumbbell-6nodes.txt");  //read topology
  topologyReader.Read ();

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute::PerOutFaceLimits","Limit","ns3::ndn::Limits::Rate","SelectSatPI", "3","InterestBuffering","true");
  ndnHelper.EnableLimits(true, Seconds(0.2),1250,40);
  ndnHelper.SetContentStore ("ns3::ndn::cs::Lru", "MaxSize", "0");
  ndnHelper.SetPit ("ns3::ndn::pit::SerializedSize", "MaxSize", "0");
  ndnHelper.SetPit ("ns3::ndn::pit::SerializedSize", "MaxPitEntryLifetime", "0");
  ndnHelper.InstallAll ();


  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll ();

  // Getting containers for the consumer/producer
  Ptr<Node> consumer1 = Names::Find<Node> ("Src1");
  Ptr<Node> consumer2 = Names::Find<Node> ("Src2");
  
  Ptr<Node> producer1 = Names::Find<Node> ("Dst1");
  Ptr<Node> producer2 = Names::Find<Node> ("Dst2");

  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
  consumerHelper.SetAttribute ("Frequency", StringValue ("50")); // 50 interests a second
  consumerHelper.SetAttribute("LifeTime", StringValue("10s"));
  consumerHelper.SetAttribute("RetxTimer", StringValue("10s"));
  // consumerHelper.SetAttribute("Randomize", StringValue("exponential"));
  // consumerHelper.SetAttribute("Randomize", StringValue("uniform"));

  // on the first consumer node install a Consumer application
  // that will express interests in /dst1 namespace
  consumerHelper.SetPrefix ("/dst1");
  ApplicationContainer app1 = consumerHelper.Install (consumer1);

  // on the second consumer node install a Consumer application
  // that will express interests in /dst2 namespace

  consumerHelper.SetAttribute ("Frequency", StringValue ("55")); // 50 interests a second
  //
  consumerHelper.SetPrefix ("/dst2");
  ApplicationContainer app2 = consumerHelper.Install (consumer2);
  
  ndn::AppHelper producerHelper ("ns3::ndn::Producer");
  producerHelper.SetAttribute ("PayloadSize", StringValue("1250"));  

  // Register /dst1 prefix with global routing controller and
  // install producer that will satisfy Interests in /dst1 namespace
  ndnGlobalRoutingHelper.AddOrigins ("/dst1", producer1);
  producerHelper.SetPrefix ("/dst1");
  producerHelper.Install (producer1);

  // Register /dst2 prefix with global routing controller and
  // install producer that will satisfy Interests in /dst2 namespace
   ndnGlobalRoutingHelper.AddOrigins ("/dst2", producer2);
   producerHelper.SetPrefix ("/dst2");
   producerHelper.Install (producer2);

  // Calculate and install FIBs
  ndn::GlobalRoutingHelper::CalculateRoutes ();

  app2.Start(Seconds(1.0));
  app2.Stop(Seconds(8.0));

  Simulator::Stop (Seconds (10.0));

  time_t t = time(NULL);
  const tm* localTime = localtime(&t);
  std::stringstream s;
  s << localTime->tm_year + 1900;
  s << std::setw(2) << std::setfill('0') << localTime->tm_mon + 1;
  s << std::setw(2) << std::setfill('0') << localTime->tm_mday;
  s << std::setw(2) << std::setfill('0') << localTime->tm_hour;
  s << std::setw(2) << std::setfill('0') << localTime->tm_min;
  s << std::setw(2) << std::setfill('0') << localTime->tm_sec;

  std::string drop_trace("drop-trace-dumbbell-6nodes-DCQL.txt");
  std::string rate_trace("rate-trace-dumbbell-6nodes-DCQL.txt");
  std::string aggregate_trace("aggregate-trace-dumbbell-6nodes-DCQL.txt");
  std::string app_delay_trace("app-delays-trace-dumbbell-6nodes-DCQL.txt");

  // L2RateTracer::InstallAll ("20220621-3_drop-trace-5src-1213.txt", Seconds (0.1));
  L2RateTracer::InstallAll (s.str() + drop_trace, Seconds (0.1));
  // ndn::L3RateTracer::InstallAll("20220621-3_rate-trace-congestion-topo-5src.txt",Seconds (0.1));
  ndn::L3RateTracer::InstallAll(s.str() + rate_trace ,Seconds (0.1));
  // ndn::L3AggregateTracer::InstallAll("20220621-3_aggregate-trace-congestion-topo-5src.txt",Seconds (0.1));
  ndn::L3AggregateTracer::InstallAll(s.str() + aggregate_trace ,Seconds (0.1));
  ndn::AppDelayTracer::InstallAll(s.str() + app_delay_trace);

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
