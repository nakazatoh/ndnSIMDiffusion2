/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#include "smart-flooding.h"

#include "ns3/ndn-interest.h"
#include "ns3/ndn-pit.h"
#include "ns3/ndn-pit-entry.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/boolean.h"

#include <boost/ref.hpp>
#include <boost/foreach.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
namespace ll = boost::lambda;

namespace ns3 {
namespace ndn {
namespace fw {

NS_OBJECT_ENSURE_REGISTERED (SmartFlooding);

LogComponent SmartFlooding::g_log = LogComponent (SmartFlooding::GetLogName ().c_str ());

std::string
SmartFlooding::GetLogName ()
{
  return super::GetLogName ()+".SmartFlooding";
}

TypeId
SmartFlooding::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::fw::SmartFlooding")
    .SetGroupName ("Ndn")
    .SetParent <GreenYellowRed> ()
    .AddConstructor <SmartFlooding> ()
    ;
  return tid;
}

SmartFlooding::SmartFlooding ()
{
}

bool
SmartFlooding::DoPropagateInterest (Ptr<Face> inFace,
                                    Ptr<Interest> interest,
                                    Ptr<pit::Entry> pitEntry)
{
  NS_LOG_FUNCTION (this);

  // Try to work out with just green faces
  bool greenOk = super::DoPropagateInterest (inFace, interest, pitEntry);
  if (greenOk)
    return true;

  int propagatedCount = 0;

  BOOST_FOREACH (const fib::FaceMetric &metricFace, pitEntry->GetFibEntry ()->m_faces.get<fib::i_metric> ())
    {
      NS_LOG_DEBUG ("Trying " << boost::cref(metricFace));
      if (metricFace.GetStatus () == fib::FaceMetric::NDN_FIB_RED) // all non-read faces are in the front of the list
        break;

      if (!TrySendOutInterest (inFace, metricFace.GetFace (), interest, pitEntry))
        {
          continue;
        }

      propagatedCount++;
    }

  NS_LOG_INFO ("Propagated to " << propagatedCount << " faces");
  return propagatedCount > 0;
}

} // namespace fw
} // namespace ndn
} // namespace ns3
