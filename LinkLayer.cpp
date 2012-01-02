#include "LinkLayer.h"
#include "Node.h"
#include <cstdlib>
#include <cstring>

LinkLayer::LinkLayer(Node* pNode, MacSublayer* pMacSublayer):
  Layer(pNode),
  mpMacSublayer(pMacSublayer),
  mTimersStarted(0),
  mTimersFinished(0),
  mLastDestination(-1),
  mIsZombie(false)
{ }

void LinkLayer::timer(long long id)
{
  ++mTimersFinished;
  if (mIsZombie)
  {
    if (mTimersFinished == mTimersStarted) delete this;
    return;
  }
  auto it = mTimerToConnection.find(id);
  auto addressAndConnectionPtr = it->second;
  mTimerToConnection.erase(it);
  if (addressAndConnectionPtr.second->timer == id)
  {
    if (addressAndConnectionPtr.second->timeouts < MAX_RETRIES)
    {
      toMacSublayer(addressAndConnectionPtr.first,
                    addressAndConnectionPtr.second);
    }
    else
    {
      info("Ryšys su %llx nutrauktas.\n", addressAndConnectionPtr.first);
      *(addressAndConnectionPtr.second) = Connection();
    }
  }
}

void LinkLayer::fromMacSublayer(MacAddress source, Frame& rFrame)
{
  if (rFrame.length == 0)
  {
    info("Gautas tuščias kadras.\n");
    return;
  }
  ControlByte controlByte = rFrame.data[0];
  info("Gautas kadras nuo %llx (tipas %hhu, Seq %hhu, Ack %hhu)\n", source,
        controlByte.type, controlByte.seq, controlByte.ack);
  if (controlByte.type == 2)
  {
    info("Gautas visiems skirtas kadras nuo %llx.\n", source);
    mpNode->toNetworkLayer(rFrame.data + 1, rFrame.length - 1);
    return;
  }
  Connection& rConnection = mConnections[source];
  if (controlByte == 0 && rFrame.length == 1)
  { // inicializuoja susijungimą
    if (rConnection.controlByte.type != 0)
    {
      info("%llx nori prisijungti iš naujo.\n", source);
      rConnection.framePtrQueue.push_front(new Frame(1));
      mLastDestination = -1;
    }
    else info("%llx nori prisijungti.\n", source);
    rConnection.controlByte = 1;
    toMacSublayer(source, &rConnection);
  }
  else if (rConnection.controlByte == 0)
  {
    if (controlByte == 1 && rFrame.length == 1)
    {
      info("%llx patvirtino prisijungimą.\n", source);
      rConnection.controlByte.type = 1;
      rConnection.controlByte.ack++;
      rConnection.controlByte.seq++;
      delete rConnection.framePtrQueue.front();
      rConnection.framePtrQueue.pop_front();
      toMacSublayer(source, &rConnection);
    }
    else info("%llx nepatvirtino prisijungimo, bet kažką siuntė.\n", source);
  }
  else if (controlByte.type != 1)
  {
    info("%llx atsiuntė netinkamo tipo kadrą (tipas %hhu, ilgis %d).\n",
         source, controlByte.type, rFrame.length);
  }
  else if (controlByte.ack == rConnection.controlByte.seq)
  { // nepatvirtino
    if (controlByte.seq == rConnection.controlByte.ack)
    { // naujas kadras
      rConnection.controlByte.ack++;
      if (rConnection.framePtrQueue.empty())
      { // o ir neturėjo patvirtinti
        info("Naujas kadras nuo %llx.\n", source);
      }
      else info("%llx nepatvirtino, tačiau atsiuntė naują kadrą.\n", source);
      mpNode->toNetworkLayer(rFrame.data + 1, rFrame.length - 1);
    }
    else info("%llx nepatvirtino ir atsiuntė seną kadrą.\n", source);
    if (rFrame.length > 1) needsAck(source, &rConnection);
    else info("%llx atsiuntė tuščią kadrą, nors taip neturėtų būti.\n", source);
  }
  else if (--(controlByte.ack) == rConnection.controlByte.seq)
  { // patvirtino
    if (rConnection.framePtrQueue.empty())
    {
      info("%llx patvirtino, nors patvirtinimo nelaukė jog kadras.\n", source);
    }
    else
    {
      if (rConnection.controlByte == 1) rConnection.controlByte.type = 1;
      rConnection.controlByte.seq++;
      delete rConnection.framePtrQueue.front();
      rConnection.framePtrQueue.pop_front();
      rConnection.timer = 0;
      rConnection.timeouts = 0;
      if (controlByte.seq == rConnection.controlByte.ack)
      { // naujas kadras
        if (rFrame.length > 1)
        {
          info("%llx patvirtino ir atsiuntė naują kadrą.\n", source);
          rConnection.controlByte.ack++;
          needsAck(source, &rConnection);
        }
        else
        {
          info("%llx nurodė naują Seq, tačiau paketo neatsiuntė.\n", source);
        }
      }
      else if (rFrame.length > 1)
      {
        info("%llx pridėjo paketą, nors pagal Seq jo neturėjo būti.\n", source);
      }
      else
      {
        if (rConnection.framePtrQueue.empty())
        {
          info ("%llx patvirtino ir nieko neatsiuntė. Eilė tuščia.\n", source);
        }
        else
        {
          info("%llx patvirtino ir nieko atsiuntė. Eilėje dar %u kadrų.\n",
               source, rConnection.framePtrQueue.size());
          toMacSublayer(source, &rConnection);
        }
      }
    }
  }
  else info("%llx atsiuntė netikėtą kadrą (tipas %hhu, Seq %hhu, Ack %hhu).\n",
            source, controlByte.type, controlByte.seq, ++(controlByte.ack));
}

bool LinkLayer::fromNetworkLayer(MacAddress destination, Byte* packet,
                                 FrameLength packetLength)
{
  info("Tinklo lygis perdavė %hu dydžio paketą, adresuotą %llx.\n",
       packetLength, destination);
  if (packetLength > MAX_DATA_LENGTH - 1)
  {
    info("Paketas per didelis.\n");
    return false;
  }
  Connection& rConnection = mConnections[destination];
  if (destination == BROADCAST_MAC && !rConnection.framePtrQueue.empty())
  {
    rConnection.controlByte.type = 2;
    delete rConnection.framePtrQueue.back();
    rConnection.framePtrQueue.pop_back();
  }
  if (rConnection.framePtrQueue.size() >= MAX_FRAME_QUEUE_SIZE)
  {
    info("Siuntimo į %llx eilė pilna.\n", destination);
    return false;
  }
  rConnection.framePtrQueue.push_back(new Frame(packetLength + 1));
  memcpy(rConnection.framePtrQueue.back()->data + 1, packet, packetLength);
  if (rConnection.timeouts == 0)
  {
    toMacSublayer(destination, &rConnection);
  }
  return true;
}

void LinkLayer::startTimer(MacAddress destination, Connection* pConnection,
                           bool ack)
{
  if (destination == BROADCAST_MAC)
  {
    info("Laikmatis nepaleidžiamas, kadangi siunčiama visiems.\n");
    return;
  }
  mTimerToConnection.insert(make_pair(++mTimersStarted,
                                      make_pair(destination, pConnection)));
  pConnection->timer = mTimersStarted;
  if (ack)
  {
    mpNode->startTimer(this, ACK_TIMEOUT, mTimersStarted);
    info("Už nedaugiau nei %d ms išsiųs patvirtinimą.\n", ACK_TIMEOUT);
    return;
  }
  if (pConnection->timeouts == 0)
  {
    pConnection->lastDuration = max(pConnection->lastDuration
                                    - FRAME_TIMEOUT_DECR, MIN_FRAME_TIMEOUT);
  }
  else
  {
    pConnection->lastDuration += rand() % (pConnection->lastDuration << 1);
  }
  if (pConnection->lastDuration > MAX_FRAME_TIMEOUT)
  {
    pConnection->lastDuration = MAX_FRAME_TIMEOUT;
  }
  ++(pConnection->timeouts);
  info("Patvirtinimo lauksim %d ms.\n", pConnection->lastDuration);
  mpNode->startTimer(this, pConnection->lastDuration, mTimersStarted);
}

void LinkLayer::selfDestruct()
{
  if (mTimersStarted != mTimersFinished) mIsZombie = true;
  else delete this;
}

void LinkLayer::toMacSublayer(MacAddress destination, Connection* pConnection)
{
  if (pConnection->framePtrQueue.empty()) // reikia siųsti tik Ack
  {
    pConnection->framePtrQueue.push_back(new Frame(1));
    Frame* pFrame = pConnection->framePtrQueue.front();
    ControlByte controlByte = pConnection->controlByte;
    controlByte.seq--;
    pFrame->data[0] = controlByte;
    info("Siunčia Ack į %llx (tipas %hhu, Seq %hhu, Ack %hhu)\n", destination,
         controlByte.type, controlByte.seq, controlByte.ack);
    mpMacSublayer->fromLinkLayer(destination, pFrame);
  }
  else
  {
    startTimer(destination, pConnection); // galėtų būti vėliau, bet kad
                                          // neištrintų atjungus laidą
    if (mLastDestination == destination
       && mLastControlByte == pConnection->controlByte)
    {
      mpMacSublayer->sendBuffer();
      info("Siunčia pakartotonai (tipas %hhu, Seq %hhu, Ack %hhu)\n",
           mLastControlByte.type, mLastControlByte.seq, mLastControlByte.ack);
    }
    else
    {
      mLastControlByte = pConnection->controlByte;
      Frame* pFrame = pConnection->framePtrQueue.front();
      if (pFrame->length > 0) pFrame->data[0] = mLastControlByte;
      info("Siunčia į %llx (tipas %hhu, Seq %hhu, Ack %hhu)\n", destination,
           mLastControlByte.type, mLastControlByte.seq, mLastControlByte.ack);
      mpMacSublayer->fromLinkLayer(destination, pFrame);
    }
  }
}

void LinkLayer::needsAck(MacAddress destination, Connection* pConnection)
{
  if (pConnection->framePtrQueue.empty())
  {
    info("Nėra ką siųsti. Jei greitai neatsiras, siųsim tik Ack.\n");
    startTimer(destination, pConnection, true);
  }
  else
  {
    info("Prie Ack prikabinam kadrą eilėje.\n");
    toMacSublayer(destination, pConnection);
  }
}
