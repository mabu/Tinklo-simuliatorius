#ifndef HASHES_H
#define HASHES_H

namespace std
{
  template<>
  struct hash<pair<IpAddress, unsigned short> >
  {
    size_t operator()(pair<IpAddress, unsigned short> x) const throw()
    {
      return hash<long long>()(((long long)(x.first) << sizeof(unsigned short))
                               + x.second);
    }
  };
}

#endif
