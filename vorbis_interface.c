/* LAME interface to libvorbis */

#ifdef HAVEVORBIS
#include <time.h>
#include "vorbis/codec.h"
#include "vorbis/modes.h"
#include "lame.h"
#include "util.h"

int16_t convbuffer[4096]; /* take 8k out of the data segment, not the stack */
int convsize;

ogg_sync_state   oy; /* sync and verify incoming physical bitstream */
ogg_stream_state os; /* take physical pages, weld into a logical
			stream of packets */
ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
ogg_packet       op; /* one raw packet of data for decode */

vorbis_info      vi; /* struct that stores all the static vorbis bitstream
			settings */
vorbis_comment   vc; /* struct that stores all the bitstream user comments */
vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
vorbis_block     vb; /* local working space for packet->PCM decode */



int lame_decode_ogg_initfile(FILE *fd,mp3data_struct *mp3data)
{

  
  char *buffer;
  int  bytes;
  int i;


  /********** Decode setup ************/

  ogg_sync_init(&oy); /* Now we can read pages */
  

  /* grab some data at the head of the stream.  We want the first page
     (which is guaranteed to be small and only contain the Vorbis
     stream initial header) We need the first page to get the stream
     serialno. */
  
  /* submit a 4k block to libvorbis' Ogg layer */
  buffer=ogg_sync_buffer(&oy,4096);
  bytes=fread(buffer,1,4096,fd);
  ogg_sync_wrote(&oy,bytes);
  
  /* Get the first page. */
  if(ogg_sync_pageout(&oy,&og)!=1){
    /* error case.  Must not be Vorbis data */
    fprintf(stderr,"Error initializing Ogg bitstream data.\n");
    return -1;
  }
  
  /* Get the serial number and set up the rest of decode. */
  /* serialno first; use it to set up a logical stream */
  ogg_stream_init(&os,ogg_page_serialno(&og));
  
  /* extract the initial header from the first page and verify that the
     Ogg bitstream is in fact Vorbis data */
  
  /* I handle the initial header first instead of just having the code
     read all three Vorbis headers at once because reading the initial
     header is an easy way to identify a Vorbis bitstream and it's
     useful to see that functionality seperated out. */
  
  vorbis_info_init(&vi);
  vorbis_comment_init(&vc);
  if(ogg_stream_pagein(&os,&og)<0){ 
    /* error; stream version mismatch perhaps */
    fprintf(stderr,"Error reading first page of Ogg bitstream data.\n");
    return -1;
  }
  
  if(ogg_stream_packetout(&os,&op)!=1){ 
    /* no page? must not be vorbis */
    fprintf(stderr,"Error reading initial header packet.\n");
    return -1;
  }
  
  if(vorbis_synthesis_headerin(&vi,&vc,&op)<0){ 
    /* error case; not a vorbis header */
    fprintf(stderr,"This Ogg bitstream does not contain Vorbis "
	    "audio data.\n");
    return -1;
  }
  
  /* At this point, we're sure we're Vorbis.  We've set up the logical
     (Ogg) bitstream decoder.  Get the comment and codebook headers and
     set up the Vorbis decoder */
  
  /* The next two packets in order are the comment and codebook headers.
     They're likely large and may span multiple pages.  Thus we reead
     and submit data until we get our two pacakets, watching that no
     pages are missing.  If a page is missing, error out; losing a
     header page is the only place where missing data is fatal. */
  
  i=0;
  while(i<2){
    while(i<2){
      int result=ogg_sync_pageout(&oy,&og);
      if(result==0)break; /* Need more data */
      /* Don't complain about missing or corrupt data yet.  We'll
	 catch it at the packet output phase */
      if(result==1){
	ogg_stream_pagein(&os,&og); /* we can ignore any errors here
				       as they'll also become apparent
				       at packetout */
	while(i<2){
	  result=ogg_stream_packetout(&os,&op);
	  if(result==0)break;
	  if(result==-1){
	    /* Uh oh; data at some point was corrupted or missing!
	       We can't tolerate that in a header.  Die. */
	    fprintf(stderr,"Corrupt secondary header.  Exiting.\n");
	    return -1;
	  }
	  vorbis_synthesis_headerin(&vi,&vc,&op);
	  i++;
	}
      }
    }
    /* no harm in not checking before adding more */
    buffer=ogg_sync_buffer(&oy,4096);
    bytes=fread(buffer,1,4096,fd);
    if(bytes==0 && i<2){
      fprintf(stderr,"End of file before finding all Vorbis headers!\n");
      return -1;
    }
    ogg_sync_wrote(&oy,bytes);
  }
  
  /* Throw the comments plus a few lines about the bitstream we're
     decoding */
  {
    /*
    char **ptr=vc.user_comments;
    while(*ptr){
      fprintf(stderr,"%s\n",*ptr);
      ++ptr;
    }
    fprintf(stderr,"\nBitstream is %d channel, %ldHz\n",vi.channels,vi.rate);
    fprintf(stderr,"Encoded by: %s\n\n",vc.vendor);
    */
  }
  
  
  /* OK, got and parsed all three headers. Initialize the Vorbis
     packet->PCM decoder. */
  vorbis_synthesis_init(&vd,&vi); /* central decode state */
  vorbis_block_init(&vd,&vb);     /* local state for most of the decode
				     so multiple block decodes can
				     proceed in parallel.  We could init
				     multiple vorbis_block structures
				     for vd here */
  
  mp3data->stereo = vi.channels;
  mp3data->samplerate = vi.rate;
  mp3data->bitrate = 0; //ov_bitrate_instant(&vf);
  mp3data->nsamp=MAX_U_32_NUM;
  
  return 0;
}



/*
  For lame_decode_fromfile:  return code
  -1     error, or eof
  0     ok, but need more data before outputing any samples
  n     number of samples output.  
*/
int lame_decode_ogg_fromfile(FILE *fd,short int pcm_l[],short int pcm_r[],mp3data_struct *mp3data)
{
  int samples,result,i,j,eof=0,eos=0,bout=0;
  double **pcm;


  while(1){

    /* 
    **pcm is a multichannel double vector.  In stereo, for
    example, pcm[0] is left, and pcm[1] is right.  samples is
    the size of each channel.  Convert the float values
    (-1.<=range<=1.) to whatever PCM format and write it out */
    /* unpack the buffer, if it has at least 1024 samples */
    convsize=1024;
    samples=vorbis_synthesis_pcmout(&vd,&pcm);
    if (samples >= convsize || eos || eof) {
      /* read 1024 samples, or if eos, read what ever is in buffer */
      int clipflag=0;
      bout=(samples<convsize?samples:convsize);
      
      /* convert doubles to 16 bit signed ints (host order) and
	 interleave */
      for(i=0;i<vi.channels;i++){
	double  *mono=pcm[i];
	for(j=0;j<bout;j++){
	  int val=mono[j]*32767.;
	  /* might as well guard against clipping */
	  if(val>32767){
	    val=32767;
	    clipflag=1;
	  }
	  if(val<-32768){
	    val=-32768;
	    clipflag=1;
	  }
	  if (i==0) pcm_l[j]=val;
	  if (i==1) pcm_r[j]=val;
	}
      }
      
      /*
      if(clipflag)
	fprintf(stderr,"Clipping in frame %ld\n",vd.sequence);
      */
      
      /* tell libvorbis how many samples we actually consumed */
      vorbis_synthesis_read(&vd,bout); 
      
      break;
    }    






    result=ogg_sync_pageout(&oy,&og);
      
    if(result==0) {
      /* need more data */
    }else if (result==-1){ /* missing or corrupt data at this page position */
      fprintf(stderr,"Corrupt or missing data in bitstream; "
	      "continuing...\n");
    }else{
      /* decode this page */
      ogg_stream_pagein(&os,&og); /* can safely ignore errors at
				       this point */
      do {
	result=ogg_stream_packetout(&os,&op);
	if(result==0) {
	  /* need more data */
	} else if(result==-1){ /* missing or corrupt data at this page position */
	  /* no reason to complain; already complained above */
	}else{
	  /* we have a packet.  Decode it */
	  vorbis_synthesis(&vb,&op);
	  vorbis_synthesis_blockin(&vd,&vb);
	}
      } while (result!=0);
    }

    /* is this the last page? */    
    if(ogg_page_eos(&og))eos=1;
    
    if(!eos){
      char *buffer;
      int bytes;
      buffer=ogg_sync_buffer(&oy,4096);
      bytes=fread(buffer,1,4096,fd);
      ogg_sync_wrote(&oy,bytes);
      if(bytes==0)eof=1;
    }
  }
  


  mp3data->stereo = vi.channels;
  mp3data->samplerate = vi.rate;
  mp3data->bitrate = 0; //ov_bitrate_instant(&vf);
  /*  mp3data->nsamp=MAX_U_32_NUM;*/

  
  if (bout==0) {
    /* clean up this logical bitstream; before exit we see if we're
       followed by another [chained] */
    ogg_stream_clear(&os);
    
    /* ogg_page and ogg_packet structs always point to storage in
       libvorbis.  They're never freed or manipulated directly */
    
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_info_clear(&vi);  /* must be called last */
    
    /* OK, clean up the framer */
    ogg_sync_clear(&oy);
    return -1;
  }

  
  return bout;
}













ogg_stream_state os2; /* take physical pages, weld into a logical
			stream of packets */
ogg_page         og2; /* one Ogg bitstream page.  Vorbis packets are inside */
ogg_packet       op2; /* one raw packet of data for decode */

vorbis_info      vi2; /* struct that stores all the static vorbis bitstream
			settings */
vorbis_comment   vc2; /* struct that stores all the bitstream user comments */
vorbis_dsp_state vd2; /* central working state for the packet->PCM decoder */
vorbis_block     vb2; /* local working space for packet->PCM decode */




int lame_encode_ogg_init(lame_global_flags *gfp)
{
  lame_internal_flags *gfc=gfp->internal_flags;

  
  /********** Encode setup ************/
  
  /* choose an encoding mode */
  /* (mode 0: 44kHz stereo uncoupled, roughly 128kbps VBR) */
  memcpy(&vi2,&info_A,sizeof(vi));
  vi2.channels = gfc->stereo;
  vi2.rate = gfp->out_samplerate;

  
  /* add a comment */
  vorbis_comment_init(&vc2);
  vorbis_comment_add(&vc2,"Track encoded using L.A.M.E. libvorbis interface.");
  
  /* set up the analysis state and auxiliary encoding storage */
  vorbis_analysis_init(&vd2,&vi2);
  vorbis_block_init(&vd2,&vb2);
  
  /* set up our packet->stream encoder */
  /* pick a random serial number; that way we can more likely build
     chained streams just by concatenation */
  srand(time(NULL));
  ogg_stream_init(&os2,rand());
  
  /* Vorbis streams begin with three headers; the initial header (with
     most of the codec setup parameters) which is mandated by the Ogg
     bitstream spec.  The second header holds any comment fields.  The
     third header holds the bitstream codebook.  We merely need to
     make the headers, then pass them to libvorbis one at a time;
     libvorbis handles the additional Ogg bitstream constraints */
  
  {
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;
    
    vorbis_analysis_headerout(&vd2,&vc2,&header,&header_comm,&header_code);
    ogg_stream_packetin(&os2,&header); /* automatically placed in its own
					 page */
    ogg_stream_packetin(&os2,&header_comm);
    ogg_stream_packetin(&os2,&header_code);
    
    /* no need to write out here.  We'll get to that in the main loop */
  }
  
  return 0;
}



int lame_encode_ogg_finish(lame_global_flags *gfp,
			  char *mp3buf, int mp3buf_size)
{
  int eos=0,bytes=0;

  vorbis_analysis_wrote(&vd2,0);

  while(vorbis_analysis_blockout(&vd2,&vb2)==1){
    
    /* analysis */
    vorbis_analysis(&vb2,&op2);

      /* weld the packet into the bitstream */
      ogg_stream_packetin(&os2,&op2);

      /* write out pages (if any) */
      while(!eos){
	int result=ogg_stream_pageout(&os2,&og2);
	if(result==0)break;


	/* check if mp3buffer is big enough for the output */
	bytes += og.header_len + og.body_len;
	if (bytes > mp3buf_size && mp3buf_size>0)
	  return -5;
	
	memcpy(mp3buf,og.header,og.header_len);
	memcpy(mp3buf+og.header_len,og.body,og.body_len);

	/* this could be set above, but for illustrative purposes, I do
	   it here (to show that vorbis does know where the stream ends) */
	if(ogg_page_eos(&og2))eos=1;

      }
    }


  /* clean up and exit.  vorbis_info_clear() must be called last */
  ogg_stream_clear(&os2);
  vorbis_block_clear(&vb2);
  vorbis_dsp_clear(&vd2);

  
  /* ogg_page and ogg_packet structs always point to storage in
     libvorbis.  They're never freed or manipulated directly */
  return bytes;

}



int lame_encode_ogg_frame(lame_global_flags *gfp,
			  short int inbuf_l[],short int inbuf_r[],
			  char *mp3buf, int mp3buf_size)
{

  lame_internal_flags *gfc=gfp->internal_flags;
  long i;
  int eos=0;
  int bytes=0;
  
  /* expose the buffer to submit data */
  double **buffer=vorbis_analysis_buffer(&vd2,gfp->framesize);
  
  /* uninterleave samples */
  for(i=0;i<gfp->framesize;i++){
    buffer[0][i]=(double)inbuf_l[i]/32768.0;
    if (gfc->stereo==2)
      buffer[1][i]=(double)inbuf_r[i]/32768.0;
  }
  
  /* tell the library how much we actually submitted */
  vorbis_analysis_wrote(&vd2,i);

  /* vorbis does some data preanalysis, then divvies up blocks for
     more involved (potentially parallel) processing.  Get a single
     block for encoding now */
  while(vorbis_analysis_blockout(&vd2,&vb2)==1){
    int result;
    /* analysis */
    vorbis_analysis(&vb2,&op2);
    
    /* weld the packet into the bitstream */
    ogg_stream_packetin(&os,&op2);
    
    /* write out pages (if any) */
    do {
      result=ogg_stream_pageout(&os,&og2);
      if (result==0) break;
	
      /* check if mp3buffer is big enough for the output */
      bytes += og.header_len + og.body_len;
      printf("\n\n*********\ndecoded bytes=%i  %i \n",bytes,mp3buf_size);
      if (bytes > mp3buf_size && mp3buf_size>0)
	return -6;
      
      /*
	fwrite(og.header,1,og.header_len,stdout);
	fwrite(og.body,1,og.body_len,stdout);
      */
      memcpy(mp3buf,og.header,og.header_len);
      memcpy(mp3buf+og.header_len,og.body,og.body_len);
      mp3buf += og.header_len + og.body_len;
      
      if(ogg_page_eos(&og2))eos=1;
    } while (1);
  }
  gfp->frameNum++;
  return bytes;
}




#endif
