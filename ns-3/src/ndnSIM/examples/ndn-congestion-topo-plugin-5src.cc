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


std::fstream file;
int F_pitsize =0;

void
PeriodicStatsPrinter (Ptr<Node> node, Time next)
{
  Ptr<ndn::Pit> pit = node->GetObject<ndn::Pit> ();
  int pitsize;  
  pitsize = pit -> GetSize();

  //if(node -> GetId() == 16 || node -> GetId() == 17 || node -> GetId() == 18 || node -> GetId() == 19 || node -> GetId() == 20) F_pitsize = 0;
  
  //int pitsizedif;
  //pitsizedif = pitsize - F_pitsize;
  //F_pitsize = pitsize;

  /*
  std::cout << Simulator::Now ().ToDouble (Time::S) << "\t"
            << node->GetId () << "\t"
            << Names::FindName (node) << "\t"
            << pitsize << "\t"
            << pitsizedif <<"\n";
  */

  //191015 LEE write PITsize to file
  //std::fstream file;
  //file.open("210331_pitsize-congestion-topo-liner-dfcc-2src-ver2.txt",std::ios::out|std::ios::app);
  std::cout << Simulator::Now ().ToDouble (Time::S) << "\t"
       << node->GetId () << "\t"
       << Names::FindName (node) << "\t"
       << pitsize << "\n";
       //<< pitsizedif << "\n";
  //file.close();
  //


  Simulator::Schedule (next, PeriodicStatsPrinter, node, next);
}

NS_LOG_COMPONENT_DEFINE("ndn-congestion-topo-plugin-5src");

int
main (int argc, char *argv[])
{
  //LogComponentEnable("UdpEchoClientApplication",LOG_LEVEL_ALL);
  //LogComponentEnable("UdpEchoSeverApplication",LOG_LEVEL_ALL);
  //aikawa
  //Config::SetDefault("ns3::DropTailQueue::MaxPackets",StringValue("10"));
  //Config::SetDefault("ns3::ndn::Pit::PitEntryPruningTimeout",StringValue("9999"));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  AnnotatedTopologyReader topologyReader ("", 25);
  //topologyReader.SetFileName ("src/ndnSIM/examples/topologies/topo-ring-2src.txt");
  topologyReader.SetFileName ("src/ndnSIM/examples/topologies/topo-5src.txt");  //read topology
  topologyReader.Read ();

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute::PerOutFaceLimits","Limit","ns3::ndn::Limits::Rate");
  ndnHelper.EnableLimits(true, Seconds(0.2),0,1250);
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
  Ptr<Node> consumer4 = Names::Find<Node> ("Src4");
  Ptr<Node> consumer5 = Names::Find<Node> ("Src5");

  Ptr<Node> producer1 = Names::Find<Node> ("Dst1");
  Ptr<Node> producer2 = Names::Find<Node> ("Dst2");
  Ptr<Node> producer3 = Names::Find<Node> ("Dst3");
  Ptr<Node> producer4 = Names::Find<Node> ("Dst4");
  Ptr<Node> producer5 = Names::Find<Node> ("Dst5");


  

  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
  // consumerHelper.SetAttribute ("Frequency", StringValue ("200")); // 200 interests a second
  // consumerHelper.SetAttribute ("Frequency", StringValue ("9")); // 200 interests a second
  // consumerHelper.SetAttribute("Randomize", StringValue("exponential"));

  // on the first consumer node install a Consumer application
  // that will express interests in /dst1 namespace
  // consumerHelper.SetPrefix ("/dst1");
  // consumerHelper.Install (consumer1);

  // on the second consumer node install a Consumer application
  // that will express interests in /dst2 namespace

  //lee2005
  consumerHelper.SetAttribute ("Frequency", StringValue ("110"));
  //
  consumerHelper.SetPrefix ("/dst2");
  consumerHelper.Install (consumer2);

  consumerHelper.SetAttribute ("Frequency", StringValue ("11")); // 200 interests a second
  consumerHelper.SetAttribute("Randomize", StringValue("exponential"));
  consumerHelper.SetPrefix ("/dst3");
  consumerHelper.Install (consumer3);

  consumerHelper.SetAttribute ("Frequency", StringValue ("110")); // 200 interests a second
  consumerHelper.SetAttribute("Randomize", StringValue("none"));
  consumerHelper.SetPrefix ("/dst4");
  consumerHelper.Install (consumer4);

  // std::cout << "Number of Applications:" << consumer3->GetNApplications() << std::endl;
  // consumer3->GetApplication(0)->SetStopTime(Seconds(2.0));
  // consumer3->GetApplication(0)->SetStartTime(Seconds(4.0));
  // std::function nodeSetStartTime = std::mem_fn(&Application::SetStartTime);
  // Simulator::Schedule(Seconds(2.0), std::bind(std::mem_fn(&Application::SetStartTime), consumer3->GetApplication(0), Seconds(4.0)));
  // Simulator::Schedule(Seconds(2.0), &Application::SetStartTime, consumer3->GetApplication(0), Seconds(4.0));
  
  // consumerHelper.SetPrefix ("/dst5");
  // ---------------------------------------------------consumerHelper.Install (consumer5);


  
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

  // Register /dst3 prefix with global routing controller and
  // install producer that will satisfy Interests in /dst3 namespace
  ndnGlobalRoutingHelper.AddOrigins ("/dst3", producer3);
  producerHelper.SetPrefix ("/dst3");
  producerHelper.Install (producer3);

  ndnGlobalRoutingHelper.AddOrigins ("/dst4", producer4);
  producerHelper.SetPrefix ("/dst4");
  producerHelper.Install (producer4);

  ndnGlobalRoutingHelper.AddOrigins ("/dst5", producer5);
  producerHelper.SetPrefix ("/dst5");
  producerHelper.Install (producer5);

/*
  std::cout << "Time" << "\t" << "nodeID" << "\t" << "Node" << "\t"
            << "pitSize" << "\t" << "d" << "\t" << "f_pitsize" << "\t"
            << "f_rate" << "\t" << "f_pisizedif" << "\t" << "pitsizedif" << "\t"
            << "rate" << "\t" << "fromID" << "\t" << "outID"
            << "\n";
*/
//aikawa


  /*
  std::cout << "Time" << "\t"
            << "NodeId" << "\t"
            << "NodeName" << "\t"
            << "NumberOfPitEntries" << "\t"
            << "pitsize diff" << "\n";
  
*/
  //191112 write pitsize result to file 
  //file.open("20210705-topo-ring-2src.txt",std::ios::out);
  /*file << "Time" << "\t"
         << "NodeId" << "\t"
         << "NodeName" << "\t"
         << "NumberOfPITEntries" << "\t"
         << "PITsize diff" <<"\n";
  file.close();*/
  //

/*
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Dst1"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Dst2"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Dst3"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Dst4"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Dst5"), Seconds (0.1));
*/  /*
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Rtr11"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Rtr10"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Rtr9"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Rtr8"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Rtr7"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Rtr6"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Rtr5"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Rtr4"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Rtr3"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Rtr2"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Rtr1"), Seconds (0.1));

  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Src5"), Seconds (0.1));  
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Src4"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Src3"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Src2"), Seconds (0.1));
  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Src1"), Seconds (0.1));
*/

//  Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>("Dst2"), Seconds (0.01));



  // Calculate and install FIBs
  ndn::GlobalRoutingHelper::CalculateRoutes ();

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

  std::string drop_trace("drop-trace-5src-1213.txt");
  std::string rate_trace("rate-trace-congestion-topo-5src.txt");
  std::string aggregate_trace("aggregate-trace-congestion-topo-5src.txt");

  // L2RateTracer::InstallAll ("20220621-3_drop-trace-5src-1213.txt", Seconds (0.1));
  L2RateTracer::InstallAll (s.str() + drop_trace, Seconds (0.1));
  // ndn::L3RateTracer::InstallAll("20220621-3_rate-trace-congestion-topo-5src.txt",Seconds (0.1));
  ndn::L3RateTracer::InstallAll(s.str() + rate_trace ,Seconds (0.1));
  // ndn::L3AggregateTracer::InstallAll("20220621-3_aggregate-trace-congestion-topo-5src.txt",Seconds (0.1));
  ndn::L3AggregateTracer::InstallAll(s.str() + aggregate_trace ,Seconds (0.1));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
