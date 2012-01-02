#include "MacSublayer.h"
#include "Node.h"
#include <algorithm>

#define WHOLE_FRAME_ARRIVED mInputBuffer.size() \
                            == FRAME_START \
                              + max(MIN_DATA_LENGTH, mLength) * 8 \
                              + CHECKSUM_LENGTH

MacSublayer::MacSublayer(Node* pNode):
  Layer(pNode),
  mConsequentOnes(0),
  mLastVoltage(0),
  mPreambleBits(0),
  mTimersRunning(0),
  mReceivingData(false),
  mJustArrived(false),
  mIsZombie(false)
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
    info("Gavo %hu ilgio paketą nuo %llx.\n", mLength, source);
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
    mpNode->toLinkLayer(this, source, frame);
  }
  else
  {
    mpNode->startTimer(this, SIGNAL_TIMEOUT, 0);
    if (mTimersRunning < 0) mTimersRunning *= -1;
    mTimersRunning++;
  }
}

bool MacSublayer::fromLinkLayer(MacAddress destination, Frame& frame)
{
  if (frame.length > MAX_DATA_LENGTH)
  {
    info("Nori siųsti per ilgą paketą (ilgis %hu > %d).\n", frame.length,
         MAX_DATA_LENGTH);
    return false;
  }
  info("Siunčia %hu ilgio paketą į %llx.\n", frame.length, destination);
  mOutputBuffer.clear();
  mConsequentOnes = 0;
  bufferAddresss(destination);
  bufferAddresss(mpNode->macAddress());
  bufferByte((frame.length >> 8) & 0xff);
  bufferByte(frame.length & 0xff);
  for (FrameLength i = 0; i < frame.length; i++) bufferByte(frame.data[i]);
  for (FrameLength i = frame.length; i < MIN_DATA_LENGTH; i++) bufferByte(0);
  bufferChecksum();
  return sendBuffer();
}

bool MacSublayer::sendBuffer()
{
  if (mTimersRunning > 0)
  {
    info("Vyksta gavimas (%lld) – kad nebūtų kolizijos, neleista siųsti.\n",
         mTimersRunning);
    return false;
  }
  sendPreamble();
  for (bool bit : mOutputBuffer) sendBit(bit);
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

void MacSublayer::sendPreamble()
{
  mpNode->toPhysicalLayer(this, NEGATIVE_VOLTAGE);
  mpNode->toPhysicalLayer(this, POSITIVE_VOLTAGE);
  for (int i = 0; i < 6; i++)
  {
    mpNode->toPhysicalLayer(this, POSITIVE_VOLTAGE);
    mpNode->toPhysicalLayer(this, NEGATIVE_VOLTAGE);
  }
  mpNode->toPhysicalLayer(this, NEGATIVE_VOLTAGE);
  mpNode->toPhysicalLayer(this, POSITIVE_VOLTAGE);
}

void MacSublayer::sendByte(Byte byte)
{
  for (int i = 0; i < 8; i++)
  {
    sendBit(byte & (1 << (7 - i)));
  }
}

void MacSublayer::sendBit(bool bit)
{
  if (bit)
  {
    mpNode->toPhysicalLayer(this, POSITIVE_VOLTAGE);
    mpNode->toPhysicalLayer(this, NEGATIVE_VOLTAGE);
    if (5 == ++mConsequentOnes) sendBit(0);
  }
  else
  {
    mConsequentOnes = 0;
    mpNode->toPhysicalLayer(this, NEGATIVE_VOLTAGE);
    mpNode->toPhysicalLayer(this, POSITIVE_VOLTAGE);
  }
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
      info("Pastebėtas kitam gavėjui (%llx, ne %llx) skirtas paketas.\n",
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
