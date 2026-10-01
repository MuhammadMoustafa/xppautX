/* GIF encoding of an RGB picture (Kinescope's and the array plot's GIFs,
   the animator's AniGif): one GIF, or the frames of an animated one, into
   a stream the caller owns. tests/golden's kin_*.gif guard the bytes. */
#include "scrngif.h"
#include "xpp_log.h"


#include <array>
#include <cstring>
#include <vector>

namespace xpp {

#define BLOKLEN 255
#define BUFLEN 1000
#define TERMIN 'T'
#define LOOKUP 'L'
#define SEARCH 'S'
#define noOfArrays 20
 /* defines the amount of memory set aside in the encoding for the
  * LOOKUP type nodes; for a 256 color GIF, the number of LOOKUP
  * nodes will be <= noOfArrays, for a 128 color GIF the number of
  * LOOKUP nodes will be <= 2 * noOfArrays, etc.  */
#define GifPutShort(i, fout)    {fputc(i&0xff, fout); fputc(i>>8, fout);}

namespace {

struct GifTree {
  char typ;             /* terminating, lookup, or search */
  int code;             /* the code to be output */
  unsigned char ix;     /* the color map index */
  GifTree **node, *nxt, *alt;
};

struct GIFCOL {
  unsigned char r,g,b;
};

unsigned int debugFlag;
int UseGlobalMap=0;
int GifFrameDelay=5,GifFrameLoop=1000;
int chainlen = 0, maxchainlen = 0, nodecount = 0, lookuptypes = 0;
short need = 8;
GifTree *empty[256], GifRoot = {LOOKUP, 0, 0, empty, nullptr, nullptr},
        *topNode, *baseNode, **nodeArray, **lastArray;

std::array<GIFCOL,256> gifcol;
std::array<GIFCOL,256> gifGcol;
int NGlobalColors=0;

void ClearTree(int cc, GifTree *root);
unsigned char *AddCodeToBuffer(int code, short n, unsigned char *buf);

int ppmtopix(unsigned char r,unsigned char g, unsigned char b,int *n)
{
  int i,nc=*n;
  if(UseGlobalMap==1){
    for(i=0;i<NGlobalColors;i++){
    if(r==gifGcol[i].r&&g==gifGcol[i].g&&b==gifGcol[i].b)
      return i;
    }

    return -1;
  }
  for(i=0;i<nc;i++)
    if(r==gifcol[i].r&&g==gifcol[i].g&&b==gifcol[i].b)
      return i;
  if(nc>255){
    xpp::log_printf(XPP_LOG_WARN, "Too many colors \n");
    return -1;
  }
  gifcol[nc].r=r;
  gifcol[nc].g=g;
  gifcol[nc].b=b;
  nc++;
  *n=nc;
  return nc-1;
}

void local_to_global()
{
  gifcol=gifGcol;
}

int use_global_map(unsigned char *pixels,const unsigned char *ppm,int h, int w)
{
  int k=0,l=0,nc;
  for(int i=0;i<h;i++){
    for(int j=0;j<w;j++){
      const int pix=ppmtopix(ppm[k],ppm[k+1],ppm[k+2],&nc);
      if(pix<0)return(0);
      pixels[l]=pix;
      k+=3;
      l++;
    }
  }
  return(1);
}

int make_local_map(unsigned char *pixels,const unsigned char *ppm,int h, int w)
{
  int k=0,l=0;
  int ncol=0;
  for(int i=0;i<h;i++){
    for(int j=0;j<w;j++){
      const unsigned char r=ppm[k],g=ppm[k+1],b=ppm[k+2];
      k+=3;
      int pix=ppmtopix(r,g,b,&ncol);
      if(pix<0)pix=255;
      pixels[l]=pix;
      l++;
    }
  }
  xpp::log(XPP_LOG_INFO, "Got {} colors\n",ncol);
  for(int i=ncol;i<256;i++){
    gifcol[i].r=255;
    gifcol[i].g=255;
    gifcol[i].b=255;
  }
  return ncol;
}

/* the logical screen: its size and the colour map (256 colours) */
void put_screen(std::vector<unsigned char> &out, int cols, int rows, unsigned char flags)
{
  const std::array<unsigned char,7> head={
    static_cast<unsigned char>(0xff & cols),static_cast<unsigned char>((0xff00 & cols)/0x100),
    static_cast<unsigned char>(0xff & rows),static_cast<unsigned char>((0xff00 & rows)/0x100),
    flags,0xff,0x0};
  out.insert(out.end(),head.begin(),head.end());
  for(const GIFCOL &c:gifcol){
    out.push_back(0xff & c.r);
    out.push_back(0xff & c.g);
    out.push_back(0xff & c.b);
  }
}

void GifLoop(FILE *fout, unsigned int repeats)
{

  fputc(0x21, fout);
  fputc(0xFF, fout);
  fputc(0x0B, fout);
  fputs("NETSCAPE2.0", fout);

  fputc(0x03, fout);
  fputc(0x01, fout);
  GifPutShort(repeats, fout); /* repeat count */

  fputc(0x00, fout); /* terminator */
}

void write_global_header(int cols,int rows, FILE *dst)
{
  std::vector<unsigned char> out={'G','I','F','8','9','a'};
  put_screen(out,cols,rows,0x87);
  fwrite(out.data(),out.size(),1,dst);
  GifLoop(dst,GifFrameLoop);
}

void write_local_header(int cols,int rows, FILE *fout,int colflag,int delay)
{
  fputc(0x21, fout);
  fputc(0xF9, fout);
  fputc(0x04, fout);
  fputc(0x80, fout); /* flag ??? */
  GifPutShort(delay,fout);
  fputc(0x00, fout);
  fputc(0x00,fout);
  fputc(',',fout); /* image separator */
  GifPutShort(0,fout);
  GifPutShort(0,fout);
  GifPutShort(cols,fout);
  GifPutShort(rows,fout);
  if(colflag)
    fputc(0x87,fout);
  else
    fputc(0x07,fout);
  if(colflag){
    for(const GIFCOL &c:gifcol){
      fputc(0xff&c.r,fout);
      fputc(0xff&c.g,fout);
      fputc(0xff&c.b,fout);
    }
  }
}

int GifEncode(FILE *fout, unsigned char *pixels, int depth, int siz)
{
  GifTree *first = &GifRoot, *newNode, *curNode;
  unsigned char  *end;
  int     cc, eoi, next, tel=0;
  short   cLength;

  unsigned char    *pos, *buffer;

  empty[0] = NULL;
  need = 8;

  nodeArray = empty;
  memmove(++nodeArray, empty, 255*sizeof(GifTree **));
  std::vector<unsigned char> block(BUFLEN+1); /* buffer[-1] is a block's length */
  buffer = block.data()+1;


  pos = buffer;
  buffer[0] = 0x0;

  cc = (depth == 1) ? 0x4 : 1<<depth;
  fputc((depth == 1) ? 2 : depth, fout); 
  eoi = cc+1;
  next = cc+2;

  cLength = (depth == 1) ? 3 : depth+1;

  std::vector<GifTree> nodes(4094);
  std::vector<GifTree *> arrays(256*noOfArrays);
  topNode = baseNode = nodes.data();
  nodeArray = first->node = arrays.data();
  lastArray = nodeArray + ( 256*noOfArrays - cc);
  ClearTree(cc, first);

  pos = AddCodeToBuffer(cc, cLength,pos);

  end = pixels+siz;
  curNode = first;
  while(pixels < end) {

    if ( curNode->node[*pixels] != NULL ) {
      curNode = curNode->node[*pixels];
      tel++;
      pixels++;
      chainlen++;
      continue;
    } else if ( curNode->typ == SEARCH ) {
      newNode = curNode->nxt;
      while ( newNode->alt != NULL ) {
	if ( newNode->ix == *pixels ) break;
	newNode = newNode->alt;
      }
      if (newNode->ix == *pixels ) {
	tel++;
	pixels++;
	chainlen++;
	curNode = newNode;
	continue;
      }
    }

/* ******************************************************
 * If there is no more thread to follow, we create a new node.  If the
 * current node is terminating, it will become a SEARCH node.  If it is
 * a SEARCH node, and if we still have room, it will be converted to a
 * LOOKUP node.
*/
  newNode = ++topNode;
  switch (curNode->typ ) {
   case LOOKUP:
     newNode->nxt = NULL;
     newNode->alt = NULL,
     curNode->node[*pixels] = newNode;
   break;
   case SEARCH:
     if ( nodeArray != lastArray ) {
       nodeArray += cc;
       curNode->node = nodeArray;
       curNode->typ = LOOKUP;
       curNode->node[*pixels] = newNode;
       curNode->node[(curNode->nxt)->ix] = curNode->nxt;
       lookuptypes++;
       newNode->nxt = NULL;
       newNode->alt = NULL,
       curNode->nxt = NULL;
       break;
     }
/*   otherwise do as we do with a TERMIN node  */
   case TERMIN:
     newNode->alt = curNode->nxt;
     newNode->nxt = NULL,
     curNode->nxt = newNode;
     curNode->typ = SEARCH;
     break;
   default:
     xpp::log_printf(XPP_LOG_WARN, "Silly node type: %d\n", curNode->typ);
  }
  newNode->code = next;
  newNode->ix = *pixels;
  newNode->typ = TERMIN;
  newNode->node = empty;
  nodecount++;
/*
* End of node creation
* ******************************************************
*/
  if (debugFlag) {
    if (curNode == newNode) xpp::log_printf(XPP_LOG_WARN, "Wrong choice of node\n");
    if ( curNode->typ == LOOKUP && curNode->node[*pixels] != newNode ) xpp::log_printf(XPP_LOG_WARN, "Wrong pixel coding\n");
    if ( curNode->typ == TERMIN ) xpp::log_printf(XPP_LOG_WARN, "Wrong Type coding; pixel# = %d; nodecount = %d\n", tel, nodecount);
  }
    pos = AddCodeToBuffer(curNode->code, cLength, pos);
    if ( chainlen > maxchainlen ) maxchainlen = chainlen;
    chainlen = 0;
    if(pos-buffer>BLOKLEN) {
      buffer[-1] = BLOKLEN;
      fwrite(buffer-1, 1, BLOKLEN+1, fout);
      buffer[0] = buffer[BLOKLEN];
      buffer[1] = buffer[BLOKLEN+1];
      buffer[2] = buffer[BLOKLEN+2];
      buffer[3] = buffer[BLOKLEN+3];
      pos -= BLOKLEN;
    }
    curNode = first;

    if(next == (1<<cLength)) cLength++;
    next++;

    if(next == 0xfff) {
      ClearTree(cc,first);
      pos = AddCodeToBuffer(cc, cLength, pos);
      if(pos-buffer>BLOKLEN) {
	buffer[-1] = BLOKLEN;
	fwrite(buffer-1, 1, BLOKLEN+1, fout);
	buffer[0] = buffer[BLOKLEN];
	buffer[1] = buffer[BLOKLEN+1];
	buffer[2] = buffer[BLOKLEN+2];
	buffer[3] = buffer[BLOKLEN+3];
	pos -= BLOKLEN;
      }
      next = cc+2;
      cLength = (depth == 1)?3:depth+1;
    }
  }

  pos = AddCodeToBuffer(curNode->code, cLength, pos);
  if(pos-buffer>BLOKLEN-3) {
    buffer[-1] = BLOKLEN-3;
    fwrite(buffer-1, 1, BLOKLEN-2, fout);
    buffer[0] = buffer[BLOKLEN-3];
    buffer[1] = buffer[BLOKLEN-2];
    buffer[2] = buffer[BLOKLEN-1];
    buffer[3] = buffer[BLOKLEN];
    buffer[4] = buffer[BLOKLEN+1];
    pos -= BLOKLEN-3;
  }
  pos = AddCodeToBuffer(eoi, cLength, pos);
  pos = AddCodeToBuffer(0x0, -1, pos);
  buffer[-1] = pos-buffer;
   pos = AddCodeToBuffer(0x0,8,pos);

  fwrite(buffer-1, pos-buffer+1, 1, fout);
  first->node = empty; /* arrays goes with this call */
  if (debugFlag) xpp::log_printf(XPP_LOG_DEBUG, "pixel count = %d; nodeCount = %d lookup nodes = %d\n", tel, nodecount, lookuptypes);
  return 1;

}

void ClearTree(int cc, GifTree *root)
{
  int i;
  GifTree *newNode, **xx;

  if (debugFlag>1) xpp::log_printf(XPP_LOG_DEBUG, "Clear Tree  cc= %d\n", cc);
  if (debugFlag>1) xpp::log_printf(XPP_LOG_DEBUG, "nodeCount = %d lookup nodes = %d\n", nodecount, lookuptypes);
  maxchainlen=0; lookuptypes = 1;
  nodecount = 0;
  nodeArray = root->node;
  xx= nodeArray;
  for (i = 0; i < noOfArrays; i++ ) {
    memmove (xx, empty, 256*sizeof(GifTree **));
    xx += 256;
  }
  topNode = baseNode;
  for(i=0; i<cc; i++) {
    root->node[i] = newNode = ++topNode;
    newNode->nxt = NULL;
    newNode->alt = NULL;
    newNode->code = i;
    newNode->ix = i;
    newNode->typ = TERMIN;
    newNode->node = empty;
    nodecount++;
  }
}

unsigned char *AddCodeToBuffer(int code, short n, unsigned char *buf)
{
  int    mask;

  if(n<0) {
    if(need<8) {
      buf++;
      *buf = 0x0;
    }
    need = 8;
    return buf;
  }

  while(n>=need) {
    mask = (1<<need)-1;
    *buf += (mask&code)<<(8-need);
    buf++;
    *buf = 0x0;
    code = code>>need;
    n -= need;
    need = 8;
  }
  if(n) {
    mask = (1<<n)-1;
    *buf += (mask&code)<<(8-need);
    need -= n;
  }
  return buf;
}

void make_gif(unsigned char *pixels,int cols,int rows,FILE *dst)
{
  const int depth=8;
  std::vector<unsigned char> out={'G','I','F','8','7','a'};
  put_screen(out,cols,rows,static_cast<unsigned char>(0xf0 | (0x7&(depth-1))));
  const std::array<unsigned char,10> image={
    0x2c,0x00,0x00,0x00,0x00,
    static_cast<unsigned char>(0xff & cols),static_cast<unsigned char>((0xff00 & cols)/0x100),
    static_cast<unsigned char>(0xff & rows),static_cast<unsigned char>((0xff00 & rows)/0x100),
    static_cast<unsigned char>(0x7&(depth-1))};
  out.insert(out.end(),image.begin(),image.end());
  fwrite(out.data(),out.size(),1,dst);

  /* header info done */

  GifEncode(dst,pixels,depth,rows*cols);
  fputc(';',dst);
}

} // namespace

void set_global_map(int flag)
{
  if(NGlobalColors==0){  /* Cant use it if it aint there */
    UseGlobalMap=0;
    return;
  }
  UseGlobalMap=flag;
}

void end_ani_gif(FILE *fp)
{
  fputc(';',fp);
}

/* encode the w x h RGB image ppm for task (scrngif.h): one GIF, the global
   colour map, or the first/next frame of an animated GIF */
void gif_stuff_ppm(unsigned char *ppm,int w,int h,FILE *fp,int task)
{
 std::vector<unsigned char> pix(static_cast<size_t>(h)*w);
 unsigned char *pixels=pix.data();
 int ncol=0;

 int ok;
 switch(task){
 case GET_GLOBAL_CMAP:
   ncol=make_local_map(pixels,ppm,h,w);
   gifGcol=gifcol;
   NGlobalColors=ncol;
   break;
 case MAKE_ONE_GIF: /* don't need global map! */
   make_local_map(pixels,ppm,h,w);
   make_gif(pixels,w,h,fp);
   break;
 case FIRST_ANI_GIF:
   if(UseGlobalMap)
     {
       ok=use_global_map(pixels,ppm,h,w);
       if(ok==1)
	 {
	   local_to_global();
	   write_global_header(w,h,fp);
	   write_local_header(w,h,fp,0,GifFrameDelay);
	   GifEncode(fp,pixels,8,w*h);
	 }
       else /* first map cant be encoded */
	 {
           UseGlobalMap=0;
	   local_to_global();
	   write_global_header(w,h,fp);  /* write global header */
	   make_local_map(pixels,ppm,h,w);
	   write_local_header(w,h,fp,1,GifFrameDelay);
	   GifEncode(fp,pixels,8,w*h);
	   UseGlobalMap=1;

	 }
     }
   else
     {
        make_local_map(pixels,ppm,h,w);
	write_global_header(w,h,fp);
	write_local_header(w,h,fp,0,GifFrameDelay);
	GifEncode(fp,pixels,8,w*h);
     }
    break;
 case NEXT_ANI_GIF:
   if(UseGlobalMap)
     {
       ok=use_global_map(pixels,ppm,h,w);
       if(ok==1)
	 {
	   write_local_header(w,h,fp,0,GifFrameDelay);
	   GifEncode(fp,pixels,8,w*h);
	 }
       else
	 {
	   UseGlobalMap=0;
	   make_local_map(pixels,ppm,h,w);
	   write_local_header(w,h,fp,1,GifFrameDelay);
	   GifEncode(fp,pixels,8,w*h);
	   UseGlobalMap=1;
	 }
     }
   else
     {
       make_local_map(pixels,ppm,h,w);
       write_local_header(w,h,fp,1,GifFrameDelay);
       GifEncode(fp,pixels,8,w*h);
     }
   break;
 }
}

} // namespace xpp
