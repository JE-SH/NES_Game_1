
#include <stdlib.h>
#include <string.h>
// include NESLIB header
#include "neslib.h"
// include CC65 NES Header (PPU)
#include <nes.h>
// link the pattern table into CHR ROM
//#link "chr_generic.s"
// BCD arithmetic support
#include "bcd.h"
//#link "bcd.c"
// VRAM update buffer
#include "vrambuf.h"
//#link "vrambuf.c"
 
//*********************************************************************
//DEFINITIONS 
#define NUM_Objects 1
#define LIVES 3
//#define COLS 30		
//#define CH_FLOOR 0xf4

//*****INTEGERS*****//
int Walk=0; //1 si camina, 0 si no
int LorR=0; // 0 si es izquierda, 1 si es derecha
//*****ACTOR BYTES*****//
byte actor_x; //coordenada del personaje
byte actor_dx; //movimiento de personaje
//*****OTHER VARIABLES*****//
byte vel=1;
byte n_random;
byte vel_changer=6;
static byte score = 0;
static byte bad_score=LIVES+1;

//-----------------------------------------------------------------------------
///----- METASPRITES -----///
// 			2x2 metasprite
#define DEF_METASPRITE_2x2(name,code,pal)\
const unsigned char name[]={\
        0,      0,      (code)+0,   pal, \
        0,      8,      (code)+1,   pal, \
        8,      0,      (code)+2,   pal, \
        8,      8,      (code)+3,   pal, \
        128};

//			2x2 metasprite, flipped horizontally
#define DEF_METASPRITE_2x2_FLIP(name,code,pal)\
const unsigned char name[]={\
        8,      0,      (code)+0,   (pal)|OAM_FLIP_H, \
        8,      8,      (code)+1,   (pal)|OAM_FLIP_H, \
        0,      0,      (code)+2,   (pal)|OAM_FLIP_H, \
        0,      8,      (code)+3,   (pal)|OAM_FLIP_H, \
        128};

// 			2x1 metasprite width
#define DEF_METASPRITE_2x1_W(name,code,pal)\
const unsigned char name[]={\
        0,      0,      (code)+0,   pal, \
        8,      0,      (code)+1,   pal, \
        128};

// 			2x1 metasprite height
#define DEF_METASPRITE_2x1_H(name,code,pal)\
const unsigned char name[]={\
        0,      0,      (code)+0,   pal, \
        0,      8,      (code)+1,   pal, \
        128};

// 			2x1 metasprite height, flipped 
#define DEF_METASPRITE_2x1_H_F(name,code,pal)\
const unsigned char name[]={\
        0,      0,      (code)+0,   (pal)|OAM_FLIP_H, \
        0,      8,      (code)+1,   (pal)|OAM_FLIP_H, \
        128};

// right-facing
DEF_METASPRITE_2x2(playerRStand, 0xd8, 0);
DEF_METASPRITE_2x2(playerRRun1, 0xdc, 0);
DEF_METASPRITE_2x2(playerRRun2, 0xe0, 0);
DEF_METASPRITE_2x2(playerRRun3, 0xe4, 0);
DEF_METASPRITE_2x2(playerRJump, 0xe8, 0);

// left-facing
DEF_METASPRITE_2x2_FLIP(playerLStand, 0xd8, 0);
DEF_METASPRITE_2x2_FLIP(playerLRun1, 0xdc, 0);
DEF_METASPRITE_2x2_FLIP(playerLRun2, 0xe0, 0);
DEF_METASPRITE_2x2_FLIP(playerLRun3, 0xe4, 0);
DEF_METASPRITE_2x2_FLIP(playerLJump, 0xe8, 0);

// BASKET
DEF_METASPRITE_2x1_W(basket1,0x8b,0);
DEF_METASPRITE_2x1_W(basket2,0x8c,0);
// CLOUDS
DEF_METASPRITE_2x1_H(cloud1,0x86,2);
DEF_METASPRITE_2x1_H_F(cloud2,0x86,2);

//Player Walking
const unsigned char* const playerWalkSeq[16]={
  playerLRun1, playerLRun2, playerLRun3, 
  playerLRun1, playerLRun2, playerLRun3, 
  playerLRun1, playerLRun2,
  playerRRun1, playerRRun2, playerRRun3, 
  playerRRun1, playerRRun2, playerRRun3, 
  playerRRun1, playerRRun2,
};

//NOT USED BY THE MOMENT
const char ATTRIBUTE_TABLE[0x625] = {

};
/*{pal:"nes",layout:"nes"}*/
const char PALETTE[32] = { 
  0x21,			// screen color

  0x07,0x19,0x1A,0x00,	// background palette 0
  0x1C,0x20,0x2C,0x00,	// background palette 1
  0x00,0x10,0x20,0x00,	// background palette 2
  0x06,0x16,0x26,0x00,   // background palette 3

  0x16,0x35,0x24,0x00,	// sprite palette 0
  0x00,0x37,0x25,0x00,	// sprite palette 1
  0x0D,0x2D,0x3A,0x00,	// sprite palette 2
  0x0D,0x27,0x2A	// sprite palette 3
};
//-----------------------------------------------------------------------------^
//////////////////////////////////////////////////////////////
//** ESTRUCTURAS (OBJETOS)
typedef struct Objects {
  byte x;
  byte y;
  int id;
  bool notActive; // 0= Activo   1=NO ACTIVO
}Objects;

// ARRAY OF OBJECTS
Objects object[NUM_Objects];

/////////////////////////////////////////////////////////////^
//____________________________________________________________________________
// OAM OFF -*-*-*-*-*-*-*-*-*-*-*-*-*-*-
void draw_scoreboard() {
  byte i,sum;
  
  oam_off = oam_spr(24+0, 24, '0'+(score >> 4), 2, oam_off);
  oam_off = oam_spr(24+8, 24, '0'+(score & 0xf), 2, oam_off);
  
  for(i=0;i<bad_score-1;++i)
  {
    sum=(i)*(12);
    oam_meta_spr_pal(200+sum, 24, 0, playerLStand);
  }
}
// ACTOR-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
// *AVANZAR SI SE PRESIONA ALGUN BOTÓN *//
void moveActor(){
   // move actor[i] left/right
    char pad;	// controller flags  
    pad = pad_poll(0);


    if (pad&PAD_LEFT && actor_x>16)
    {
      actor_dx=-2;
      LorR=0;
      Walk=1;
    }
    else if (pad&PAD_RIGHT && actor_x<224)
    {
      actor_dx=2;
      LorR=1;
      Walk=1;
    }
   else 
    {
      actor_dx=0;
      Walk=0;
    }
}

// *TRAZADO DEL PERSONAJE* //
void drawActor()
{
  byte meta;
  meta = actor_x & 7; 
  
    if(Walk) 
    {
      if(LorR)
      	meta+=8;
    }
    else 
    {
      if(LorR==0)//izquierda
      	meta = 1;
      else
        meta = 9;
    }
    
  oam_meta_spr_pal(actor_x, 200, 0, playerWalkSeq[meta]);
  oam_meta_spr_pal(actor_x-8, 193, 0, basket1);
  oam_meta_spr_pal(actor_x+8, 193, 0, basket2);

  actor_x += actor_dx; 

}

// OBJECTS -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
// *GENERADOR DE NÚMEROS ALEATORIOS* //
void randomPlace(byte i)
{
  struct Objects* o = &object[i];
  if(o->x==0)
  {
    o->x=(rand8()%224);
  }  
}
//
bool collision(byte obj)
{
  struct Objects* o = &object[obj];
  byte notactive = o-> notActive;
  
  if(!notactive) //SI ESTÁ ACTIVO
  {
    byte oy = o->y;
    byte ox = o->x;
  
      if(oy >= 185)
      {
      //mbox=;
        if(ox >= actor_x-30 && ox <= actor_x+14)
        {
          score = bcd_add(score, 1);
          --vel_changer;
          o->notActive=1;
          
          return true;
        }
        else {
          --bad_score;
          o->notActive=1;
        }
      }
    return false;
  }
}

void changeVelocity()
{
  if(vel_changer<=0)
  {
    ++vel;
    vel_changer=6;
  }
}

// *DIBUJAR ITEMS* //
void drawItem(byte ob) 
{
 // if(obj1
  struct Objects* o = &object[ob];
  byte oy = o->y;
  
  if( oy > 0 && oy < 200){
    o->y +=vel;
    
    if(!collision(ob))
      oam_meta_spr_pal(o->x +8, o->y ,2, playerLJump);
    else
      o->x=240;
  
  }
  else{
    o->y =1;
    o->x =0;
    o->notActive=0;
  }
  
 // if(ob == 0 && oy ==100 && object[1].y == 0){
 //   object[1].y = 1;
 // }
 
  delay(1);
  //ppu_wait_frame();

}
//////////////////////////////////////////////////////////////

// setup PPU and tables
void setup_graphics() {
  // clear sprites
  oam_clear();
  // set palette colors
  pal_all(PALETTE);
  // enable rendering
  ppu_on_all();
}
void setup_objects(){
  
  byte i;
  for(i =0;i<NUM_Objects;++i)
  {
    object[i].x = 0;
    object[i].y = 0;
    object[i].notActive = 0;
    
  }
}

//*****INICIO DEL JUEGO*****//
void mainGame(){
  byte i;
  drawActor(); 	//Dibuja al actor principal
  moveActor();	//Mueve al actor
  for(i=0;i<NUM_Objects;++i){
    randomPlace(i); 	//Elige lugar aleatorio para el objeto i
    drawItem(i);		//Dibuja el objeto
  }
    changeVelocity();
}
void gameover()
{
  vram_adr(NTADR_A(2,20)); // Coordenadas/dirección para poner suelo
  vram_write("GAME OVER",9);
}
void main(void)
{
  
  setup_graphics(); // Preparación de gráficos
  //obj1_x=200;
  object[0].y=1;
  actor_x=128;	//coordenada del personaje en x al inicio del juego
  actor_dx=0;   //bytes en movimiento del personaje
  vram_adr(NTADR_A(0,27)); // Coordenadas/dirección para poner suelo
  vram_fill(0x80, 32); // Rellenar memoria con el sprite del suelo
  
  scroll(0,0);
  //CICLO INFINITO
  while(1) {

    //--- *** ---//
    if(bad_score>0)
    {
    draw_scoreboard();	//Dibuja puntaje
      
    mainGame();
      
    }
    else{ //GAMEOVER
      break;
    }
    oam_clear();	//Limpia oam
    
    //collision();
    
  }
      gameover();
  
}
