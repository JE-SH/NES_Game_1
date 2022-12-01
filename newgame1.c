
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
 
//**
//Definiciones dentro del código
#define NUM_ACTORS 24
#define COLS 30		// floor width in tiles

#define CH_FLOOR 0xf4
int Walk=0; //1 si camina, 0 si no
int LorR=0; // 0 si es izquierda, 1 si es derecha

byte actor_x; //coordenada del personaje
byte actor_dx; //movimiento de personaje
byte vel=1;
byte n_random;
byte vel_changer=6;

int obj1_y=0;
int obj2_y=0;
int obj1_x=0;
int obj2_x=0;
int obj1_id=0;
int obj2_id=0;

static byte score = 0;
static byte bad_score=3;
///----- METASPRITES -----///
// define a 2x2 metasprite
#define DEF_METASPRITE_2x2(name,code,pal)\
const unsigned char name[]={\
        0,      0,      (code)+0,   pal, \
        0,      8,      (code)+1,   pal, \
        8,      0,      (code)+2,   pal, \
        8,      8,      (code)+3,   pal, \
        128};

// define a 2x2 metasprite, flipped horizontally
#define DEF_METASPRITE_2x2_FLIP(name,code,pal)\
const unsigned char name[]={\
        8,      0,      (code)+0,   (pal)|OAM_FLIP_H, \
        8,      8,      (code)+1,   (pal)|OAM_FLIP_H, \
        0,      0,      (code)+2,   (pal)|OAM_FLIP_H, \
        0,      8,      (code)+3,   (pal)|OAM_FLIP_H, \
        128};

// define a 2x1 metasprite width
#define DEF_METASPRITE_2x1_W(name,code,pal)\
const unsigned char name[]={\
        0,      0,      (code)+0,   pal, \
        8,      0,      (code)+1,   pal, \
        128};
// define a 2x1 metasprite height
#define DEF_METASPRITE_2x1_H(name,code,pal)\
const unsigned char name[]={\
        0,      0,      (code)+0,   pal, \
        0,      8,      (code)+1,   pal, \
        128};
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

const unsigned char* const playerWalkSeq[16]={
  playerLRun1, playerLRun2, playerLRun3, 
  playerLRun1, playerLRun2, playerLRun3, 
  playerLRun1, playerLRun2,
  playerRRun1, playerRRun2, playerRRun3, 
  playerRRun1, playerRRun2, playerRRun3, 
  playerRRun1, playerRRun2,
};


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
//////////////////////////////////////////////////////////////
//** ESTRUCTURAS (OBJETOS)
typedef struct Object {
  byte x;
  byte y;
  int id;
  bool isActive;
}Object;

/////////////////////////////////////////////////////////////
bool collision(byte obj)
{
  if(obj==1){
    if(obj1_y>=185 && obj1_y<=190)
    {
      //mbox=;
      if(obj1_x>=actor_x-30 && obj1_x<=actor_x+14)
      {
        score = bcd_add(score, 1);
        --vel_changer;
        return true;
      }
      else       --bad_score;
    }
  }
  else if(obj==2)
  {
     if(obj2_y>=185&&obj2_y<=190)
     {
       if(obj2_x>=actor_x-30 && obj2_x<=actor_x+14)
      {
         score = bcd_add(score, 1);
        --vel_changer;
         return true;
      }
       else       --bad_score;
     }
  }
  return false;
}

void changeVelocity()
{
  if(vel_changer<=0)
  {
    ++vel;
    vel_changer=6;
  }
}

///////////////////////
void draw_scoreboard() {
  byte i,sum;
  oam_off = oam_spr(24+0, 24, '0'+(score >> 4), 2, oam_off);
  oam_off = oam_spr(24+8, 24, '0'+(score & 0xf), 2, oam_off);
  for(i=0;i<bad_score;++i)
  {
    sum=(i)*(12);
    oam_meta_spr_pal(200+sum, 24, 0, playerLStand);
  }
}

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

// *GENERADOR DE NÚMEROS ALEATORIOS* //
void randomPlace()
{
  if(obj1_y==0)
  {
    obj1_x=(rand8()%224);
  }
  if(obj2_y==0)
  {
    obj2_x=(rand8()%224);
  }
  
}

// *DIBUJAR ITEMS* //
void drawItem() 
{
 // if(obj1
  if(obj2_y>=100)
  {
    obj1_y+=vel;
    obj2_y+=vel;
    
    if(obj2_y >=200)
    {
      obj2_y=0;
    }
  }
  else if(obj1_y>=100)
  {
    obj2_y+=vel;
    obj1_y+=vel; 
    
    if(obj1_y >=200)
    {
      obj1_y=0;
    }
  }
  else {
    
   obj1_y+=vel; 
  }
  if(obj1_y>0)
  {
    if(!collision(1))
      oam_meta_spr_pal(obj1_x+8, obj1_y,2, playerLJump);
    else
      obj1_x=240;
  }
  if(obj2_y>0)
  {
    if(!collision(2))
      oam_meta_spr_pal(obj2_x+8, obj2_y,2, playerLJump );
     else
      obj2_x=240;
  }
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

//*****INICIO DEL JUEGO*****//
void mainGame(){
    randomPlace(); 	//Elige lugar aleatorio 
    drawActor(); 	//Dibuja al actor principal
    moveActor();	//Mueve al actor
    drawItem();		//Dibuja el objeto
    changeVelocity();
}
void main(void)
{
  
  setup_graphics(); // Preparación de gráficos
  obj1_x=200;
  actor_x=128;	//coordenada del personaje en x al inicio del juego
  actor_dx=0;   //bytes en movimiento del personaje
  vram_adr(NTADR_A(0,27)); // Coordenadas/dirección para poner suelo
  vram_fill(0x80, 32); // Rellenar memoria con el sprite del suelo
  
  scroll(0,0);
  //CICLO INFINITO
  while(1) {

    //--- *** ---//
    mainGame();
   
    oam_clear();	//Limpia oam
    
    draw_scoreboard();	//Dibuja puntaje
    //collision();
    
  }
}
