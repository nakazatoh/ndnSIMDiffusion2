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
 *     ./waf --run=ndn-3to3-dumbbell
 */


NS_LOG_COMPONENT_DEFINE("ndn-dumbbell-6nodes");

int
main (int argc, char *argv[])
{
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("100"));
  //Config::SetDefault("ns3::ndn::Pit::PitEntryPruningTimeout",StringValue("9999"));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  AnnotatedTopologyReader topologyReader ("", 25);
  topologyReader.SetFileName ("src/ndnSIM/examples/topologies/topo-3to3-dumbbell.txt");  //read topology
  topologyReader.Read ();

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute::PerOutFaceLimits","Limit","ns3::ndn::Limits::Rate","SelectSatPI", "0");
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
  Ptr<Node> consumer3 = Names::Find<Node> ("Src3");
  
  Ptr<Node> producer1 = Names::Find<Node> ("Dst1");
  Ptr<Node> producer2 = Names::Find<Node> ("Dst2");
  Ptr<Node> producer3 = Names::Find<Node> ("Dst3");

  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
  consumerHelper.SetAttribute ("Frequency", StringValue ("550")); // 50 interests a second
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

  consumerHelper.SetAttribute ("Frequency", StringValue ("500")); // 50 interests a second
  consumerHelper.SetPrefix ("/dst2");
  ApplicationContainer app2 = consumerHelper.Install (consumer2);
  consumerHelper.SetPrefix ("/dst3");
  ApplicationContainer app3 = consumerHelper.Install (consumer3);
  
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
   ndnGlobalRoutingHelper.AddOrigins ("/dst3", producer3);
   producerHelper.SetPrefix ("/dst3");
   producerHelper.Install (producer3);

  // Calculate and install FIBs
  ndn::GlobalRoutingHelper::CalculateRoutes ();

  app2.Start(Seconds(1.0));
  app2.Stop(Seconds(4.0));
  app3.Start(Seconds(3.0));
  app3.Stop(Seconds(6.0));

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

  std::string drop_trace("drop-trace-3to3-dumbbell.txt");
  std::string rate_trace("rate-trace-3to3-dumbbell.txt");
  std::string aggregate_trace("aggregate-trace-3to3-dumbbell.txt");
  std::string app_delay_trace("app-delays-trace-3to3-dumbbell.txt");

  L2RateTracer::InstallAll (s.str() + drop_trace, Seconds (0.1));
  ndn::L3RateTracer::InstallAll(s.str() + rate_trace ,Seconds (0.1));
  ndn::L3AggregateTracer::InstallAll(s.str() + aggregate_trace ,Seconds (0.1));
  ndn::AppDelayTracer::InstallAll(s.str() + app_delay_trace);

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
