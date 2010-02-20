// LAME test program
//
// Copyright (c) 2010 Robert Hegemann
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.

#include <lame.h>
#include <wchar.h>
#include <stdlib.h>


class PcmGenerator
{
  float* m_buffer_ch0;
  float* m_buffer_ch1;
  int m_size;
  float m_a;
  float m_b;

  double random()
  {
    int const range_max = 32768;
    int const range_min = -32767;
    return (double)rand() / (RAND_MAX + 1) * (range_max - range_min) + range_min;
  }
public:

  explicit PcmGenerator(int size)
  {    
    m_size = size >= 0 ? size : 0;
    m_buffer_ch0 = new float [m_size];
    m_buffer_ch1 = new float [m_size];
    m_a = 0;
    m_b = 0;
    advance(0);
  }

  ~PcmGenerator()
  {
    delete[] m_buffer_ch0;
    delete[] m_buffer_ch1;
  }

  float const* ch0() const { return m_buffer_ch0; }
  float const* ch1() const { return m_buffer_ch1; }

  void advance( int x ) {
    float a = m_a;
    float b = m_b;
    for (int i = 0; i < m_size; ++i) {
      a += 10;
      if (a > 32768) a = random();
      b -= 10;
      if (b < -32767) b = random();
      m_buffer_ch0[i] = a;
      m_buffer_ch1[i] = b;
    }
    m_a = a;
    m_b = b;
  }
};

class OutFile
{
  FILE* m_file_handle;

public:
  OutFile()
    : m_file_handle(0)
  {}

  explicit OutFile(wchar_t const* filename)
    : m_file_handle(0)
  {
    m_file_handle = _wfopen(filename, L"wb");
  }
  
  ~OutFile()
  {
    close();
  }

  bool isOpen() const {
    return 0 != m_file_handle;
  }

  void close() {
    if (isOpen()) {
      fclose(m_file_handle);
      m_file_handle = 0;
    }
  }

  void write(unsigned char const* data, int n) {
    fwrite(data, 1, n, m_file_handle);
  }
};

class Lame
{
  lame_t m_gf;
  bool m_init_params_called;
  
  void ensureInitialized() {
    if (isOpen()) {
      if (!m_init_params_called) {
        m_init_params_called = true;
        lame_init_params(m_gf);
      }
    }
  }

public:

  Lame()
    : m_gf( lame_init() ) 
    , m_init_params_called( false )
  {}

  ~Lame()
  {
    close();
  }

  void close() {
    if (isOpen()) {
      lame_close(m_gf);
      m_gf = 0;
    }
  }

  bool isOpen() const {
    return m_gf != 0;
  }

  operator lame_t () {
    return m_gf;
  }
  operator lame_t () const {
    return m_gf;
  }

  void setInSamplerate( int rate ) {
    lame_set_in_samplerate(m_gf, rate);
  }

  void setOutSamplerate( int rate ) {
    lame_set_out_samplerate(m_gf, rate);
  }

  void setNumChannels( int num_channel ) {
    lame_set_num_channels(m_gf, num_channel);
  }

  int encode(float const* ch0, float const* ch1, int n_in, unsigned char* out_buffer, int m_out_free) {
    ensureInitialized();
    return lame_encode_buffer_float(m_gf, ch0, ch1, n_in, out_buffer, m_out_free);
  }

  int flush(unsigned char* out_buffer, int m_out_free) {
    ensureInitialized();
    return lame_encode_flush(m_gf, out_buffer, m_out_free);
  }

  int getLameTag(unsigned char* out_buffer, int m_out_free) {
    ensureInitialized();
    return lame_get_lametag_frame(m_gf, out_buffer, m_out_free);
  }

};

class OutBuffer
{
  unsigned char* m_data;
  int m_size;
  int m_used;

public:
  
  OutBuffer()
  {
    m_size = 1000 * 1000;
    m_data = new unsigned char[ m_size ];
    m_used = 0;
  }

  ~OutBuffer() 
  {
    delete[] m_data;
  }

  void advance( int i ) {
    m_used += i;
  }

  int used() const {
    return m_used;
  }

  int unused() const {
    return m_size - m_used;
  }

  unsigned char* current() { return m_data + m_used; }
  unsigned char* begin()   { return m_data; }
};

void generateFile(wchar_t const* filename, size_t n)
{
  int const chunk = 1152;
  PcmGenerator src(chunk);

  OutFile mp3_stream( filename );
  if (!mp3_stream.isOpen()) return;

  Lame lame;
  if (!lame.isOpen()) return;

  OutBuffer mp3_stream_buffer;
  int rc = 0;

  lame.setInSamplerate(44100);
  lame.setOutSamplerate(44100);
  lame.setNumChannels(2);

  while (n > 0) {    
    int const m = n < chunk ? n : chunk;
    if ( n < chunk ) n = 0; else n -= chunk;
    rc = lame.encode(src.ch0(), src.ch1(), m, mp3_stream_buffer.current(), mp3_stream_buffer.unused());
    wprintf(L"rc=%d %d %d\n",rc,mp3_stream_buffer.used(),mp3_stream_buffer.unused());
    if (rc < 0) return;
    mp3_stream_buffer.advance( rc );
    src.advance(m);
  }

  rc = lame.flush(mp3_stream_buffer.current(), mp3_stream_buffer.unused());
  wprintf(L"flush rc=%d\n",rc);
  if (rc < 0) return;

  mp3_stream_buffer.advance( rc );

  int lametag_size = lame.getLameTag(0,0);
  wprintf(L"lametag_size=%d\n",lametag_size);

  rc = lame.getLameTag(mp3_stream_buffer.begin(), lametag_size);
  wprintf(L"rc=%d\n",rc);
  if (rc < 0) return;

  mp3_stream.write(mp3_stream_buffer.begin(), mp3_stream_buffer.used());

  lame.close();
}

int wmain(int argc, wchar_t** argv)
{
  if (argc != 3) {
    wprintf(L"usage: %ws <filename> <number pcm samples>\n", argv[0]);
    return -1;
  }
  wprintf(L"open file %ws\n", argv[1]);
  int n = _wtoi(argv[2]);
  wprintf(L"synthesize %d samples long mp3 file\n",n);
  generateFile(argv[1], n);
  return 0;
}
