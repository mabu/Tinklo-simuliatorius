#include "MacSublayer.h"
#include "Node.h"
#include <algorithm>
#include <cstring>

#define WHOLE_FRAME_ARRIVED mInputBuffer.size() \
                            == FRAME_START \
                              + max(MIN_DATA_LENGTH, mLength) * 8 \
                              + CHECKSUM_LENGTH

#define TO_PHYSICAL_LAYER(x) if (!toPhysicalLayer(x)) return false;

MacSublayer::MacSublayer(Node* pNode):
  Layer(pNode),
  mConsequentOnes(0),
  mLastVoltage(0),
  mPreambleBits(0),
  mTimersRunning(0),
  mReceivingData(false),
  mJustArrived(false),
  mIsZombie(false),
  mLength(0)
{ }

void MacSublayer::fromPhysicalLayer(char voltage)
{
  //info("Gavo signalą %hhd\n", voltage);
  if (voltage != NEGATIVE_VOLTAGE && voltage != POSITIVE_VOLTAGE)
  {
    info("Užfiksuota kolizija\n");
    voltage = 0;
  }
  if (mLastVoltage == 0)
  {
    mLastVoltage = voltage;
  }
  else if (mLastVoltage == voltage)
  {
    info("Bloga sinchronizacija – dvi vienodos įtampos iš eilės\n");
  } else {
    if (mLastVoltage == NEGATIVE_VOLTAGE)
    { // atkodavom 0 bitą
      if (mReceivingData && mPreambleBits != 6)
      {                  // patikrina, ar nėra įterptasis bitas
        receivedBit(0);
      }
      else if (mReceivingData && WHOLE_FRAME_ARRIVED)
      { // jei praleido įterptą 0, ir taip parsiuntė visą kadrą
        mJustArrived = true; // (checksum patikrintas su ankstesniu bitu)
      }
      if (mPreambleBits == 7)
      {
        mInputBuffer.clear();
        if (mReceivingData == true) info("Gautas nepilnas kadras.\n");
        else mReceivingData = true;
      }
      mPreambleBits = 1;
    }
    else
    { // atkodavom 1 bitą
      if (mReceivingData) receivedBit(1);
      if (mPreambleBits > 0 && mPreambleBits < 7) mPreambleBits++;
      else mPreambleBits = 0;
    }
    mLastVoltage = 0;
  }
  if (mJustArrived)
  {
    mJustArrived = false;
    mTimersRunning *= -1;
    mReceivingData = false;
    MacAddress source = 0;
    for (int i = 8 * MAC_ADDRESS_LENGTH; i < 2 * 8 * MAC_ADDRESS_LENGTH; i++)
    {
      source = (source << 1) + !!mInputBuffer[i];
    }
    info("Gavo %hu ilgio kadrą nuo %llx:\n", mLength, source);
    Frame frame(mLength);
    for (int i = 0; i < mLength; i++)
    {
      frame.data[i] = 0;
      for (int j = 0; j < 8; j++)
      {
        frame.data[i] = (frame.data[i] << 1) + !!mInputBuffer[FRAME_START
                                                              + i * 8 + j];
      }
    }
    mInputBuffer.clear();
    dumpFrame(frame);
    mpNode->toLinkLayer(this, source, frame);
  }
  else
  {
    mpNode->startTimer(this, SIGNAL_TIMEOUT, 0);
    if (mTimersRunning < 0) mTimersRunning *= -1;
    mTimersRunning++;
  }
}

bool MacSublayer::fromLinkLayer(MacAddress destination, Frame* pFrame)
{
  if (pFrame->length > MAX_DATA_LENGTH)
  {
    info("Nori siųsti per ilgą kadrą (ilgis %hu > %d).\n", pFrame->length,
         MAX_DATA_LENGTH);
    return false;
  }
  info("Siunčia %hu ilgio kadrą į %llx:\n", pFrame->length, destination);
  dumpFrame(*pFrame);
  mOutputBuffer.clear();
  mConsequentOnes = 0;
  bufferAddresss(destination);
  bufferAddresss(mpNode->macAddress());
  bufferByte((pFrame->length >> 8) & 0xff);
  bufferByte(pFrame->length & 0xff);
  for (FrameLength i = 0; i < pFrame->length; i++) bufferByte(pFrame->data[i]);
  for (FrameLength i = pFrame->length; i < MIN_DATA_LENGTH; i++) bufferByte(0);
  bufferChecksum();
  return sendBuffer();
}

bool MacSublayer::sendBuffer()
{
  if (mTimersRunning > 0)
  {
    info("Vyksta gavimas (%lld; %u) – kad nebūtų kolizijos, neleista siųsti.\n",
         mTimersRunning, mInputBuffer.size());
    return false;
  }
  if (!sendPreamble()) return false;
  for (bool bit : mOutputBuffer) if (!sendBit(bit)) return false;
  return true;
}

void MacSublayer::timer(long long id)
{
  if      (mTimersRunning > 0) --mTimersRunning;
  else if (mTimersRunning < 0) ++mTimersRunning;
  else info("Baigėsi laikmatis, nors nebuvo pradėtas.\n");
  if (mTimersRunning == 0 && mIsZombie) delete this;
}

void MacSublayer::selfDestruct()
{
  if (mTimersRunning) mIsZombie = true;
  else delete this;
}

void MacSublayer::bufferAddresss(MacAddress macAddress)
{
  for (int i = 0; i < MAC_ADDRESS_LENGTH; i++)
  {
    bufferByte((macAddress >> ((MAC_ADDRESS_LENGTH - 1 - i) * 8)) & 0xff);
  }
}

void MacSublayer::bufferByte(Byte byte)
{
  for (int i = 0; i < 8; i++)
  {
    mOutputBuffer.push_back((bool)(byte & (1 << (7 - i))));
  }
}

void MacSublayer::bufferChecksum()
{
  BitVector bufferWithChecksum = mOutputBuffer;
  bufferWithChecksum.resize(bufferWithChecksum.size() + CHECKSUM_LENGTH);
  calculateChecksum(bufferWithChecksum);
  mOutputBuffer.insert(mOutputBuffer.end(),
                       bufferWithChecksum.begin() + mOutputBuffer.size(),
                       bufferWithChecksum.end());
}

bool MacSublayer::sendPreamble()
{
  if (!mpNode->isWireIdle(this)) return false;
  TO_PHYSICAL_LAYER(NEGATIVE_VOLTAGE);
  TO_PHYSICAL_LAYER(POSITIVE_VOLTAGE);
  for (int i = 0; i < 6; i++)
  {
    TO_PHYSICAL_LAYER(POSITIVE_VOLTAGE);
    TO_PHYSICAL_LAYER(NEGATIVE_VOLTAGE);
  }
  TO_PHYSICAL_LAYER(NEGATIVE_VOLTAGE);
  TO_PHYSICAL_LAYER(POSITIVE_VOLTAGE);
  return true;
}

bool MacSublayer::sendBit(bool bit)
{
  if (!mpNode->isWireIdle(this)) return false;
  if (bit)
  {
    TO_PHYSICAL_LAYER(POSITIVE_VOLTAGE);
    TO_PHYSICAL_LAYER(NEGATIVE_VOLTAGE);
    if (5 == ++mConsequentOnes) sendBit(0);
  }
  else
  {
    mConsequentOnes = 0;
    TO_PHYSICAL_LAYER(NEGATIVE_VOLTAGE);
    TO_PHYSICAL_LAYER(POSITIVE_VOLTAGE);
  }
  return true;
}

void MacSublayer::calculateChecksum(BitVector& bufferWithChecksum)
{
  for (int i = 0; i < (int)bufferWithChecksum.size() - CHECKSUM_LENGTH; i++)
  {
    if (bufferWithChecksum[i])
    {
      for (int j = 1 ; j <= CHECKSUM_LENGTH; j++)
      {
        bufferWithChecksum[i + j] = (!bufferWithChecksum[i + j])
                                    ^
                                    (!((CRC_POLYNOMIAL >> (CHECKSUM_LENGTH - j))
                                       & 1));
      }
    }
  }
}

bool MacSublayer::isInputValid()
{
  if (mInputBuffer.size() <= CHECKSUM_LENGTH) return false;
  BitVector bufferWithChecksum = mInputBuffer;
  calculateChecksum(bufferWithChecksum);
  for (int i = 0; i < CHECKSUM_LENGTH; i++)
  {
    if (*(bufferWithChecksum.rbegin() + i) != 0)
    {
      info("Gauti duomenys sugadinti.\n");
      return false;
    }
  }
  return true;
}

bool MacSublayer::toPhysicalLayer(char voltage)
{
  if (!mpNode->isWireIdle(this))
  {
    info("Siuntimas atšauktas, kadangi pastebėta įtampa laide.\n");
    return false;
  }
  if (!mpNode->toPhysicalLayer(this, voltage))
  {
    info("Siuntimas neįvyko, kadangi laidas atsijungė.\n");
    return false;
  }
  return true;
}

void MacSublayer::receivedBit(bool bit)
{
  if (WHOLE_FRAME_ARRIVED)
  {
    mReceivingData = false;
    mInputBuffer.clear();
  }
  mInputBuffer.push_back(bit);
  if (mInputBuffer.size() == MAC_ADDRESS_LENGTH * 8)
  {
    MacAddress destination = 0;
    for (int i = 0; i < MAC_ADDRESS_LENGTH * 8; i++)
    {
      destination = (destination << 1) + !!mInputBuffer[i];
    }
    if (destination != mpNode->macAddress() && destination != BROADCAST_MAC)
    {
      info("Pastebėtas kitam gavėjui (%llx, ne %llx) skirtas kadras.\n",
           destination, mpNode->macAddress());
      mReceivingData = false;
    }
  }
  else if (mInputBuffer.size() == FRAME_START)
  {
    mLength = 0;
    for (int i = 2 * MAC_ADDRESS_LENGTH; i < (int)mInputBuffer.size(); i++)
    {
      mLength = (mLength << 1) + !!mInputBuffer[i];
    }
  }
  else if (WHOLE_FRAME_ARRIVED)
  {
    if (!isInputValid())
    {
      mReceivingData = false;
      mInputBuffer.clear();
    }
    else if (mPreambleBits != 5) mJustArrived = true;
  }
}

void MacSublayer::dumpFrame(Frame& rFrame)
{
  char string[rFrame.length * 4 + 4];
  string[0] = '\0';
  for (int i = 0; i < rFrame.length; i++)
  {
    char current[5];
    sprintf(current, " %hhu", rFrame.data[i]);
    strcat(string, current);
  }
  strcat(string, "\n");
  info(string);
}
