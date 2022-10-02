/*
  Angry Nerds
  Copyright (C) 2022 Bernhard Schelling

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <ZL_Application.h>
#include <ZL_Display.h>
#include <ZL_Surface.h>
#include <ZL_Signal.h>
#include <ZL_Audio.h>
#include <ZL_Font.h>
#include <ZL_Scene.h>
#include <ZL_Input.h>
#include <ZL_Particles.h>
#include <ZL_SynthImc.h>
#include <ZL_Thread.h>
#include <../Opt/chipmunk/chipmunk.h>
#include <vector>
#define TINYSAM_IMPLEMENTATION
#include "tinysam.h"

extern ZL_SynthImcTrack imcMusic;
extern TImcSongData imcDataIMCCANNON, imcDataIMCHIT, imcDataIMCCLEAR, imcDataIMCFAIL, imcDataIMCMUSIC;
static ZL_Font fntMain;
static ZL_TextBuffer txtBuf;
static ZL_Surface srfGround, srfWood, srfMetal, srfCannon, srfNerd, srfNerdShirt, srfSumo, srfSumoPants;
static ZL_Sound sndCannon, sndHit, sndClear, sndFail;
static ZL_ParticleEffect particleSmoke;
static tinysam* ts;
#if !defined(__SMARTPHONE__) && !defined(__WEBAPP__)
static ZL_Mutex tsmtx;
#define TSMTXLOCK() tsmtx.Lock();
#define TSMTXUNLOCK() tsmtx.Unlock();
#else
#define TSMTXLOCK()
#define TSMTXUNLOCK()
#endif
static cpSpace *space = NULL;
static cpBody *ground = NULL;
static int remainTicks;
static ticks_t ticksClear, ticksFailed;

static int level, level_sides, level_decks, remain_sumos, total_sumos;
static float level_width, level_height, level_decky[10];

static bool OnTitle = true;
static ticks_t titleswitchtick;
static float CannonRange;
static float CannonY = 150.0f;
static float CameraX = 0, CameraZoom = 1.0f;
static ZL_Vector CannonVel;
static ZL_Color colSkyTop, colSkyTopTarget, colSkyBottom, colSkyBottomTarget;

static const struct SLevelSettings { int sides, decks, rooms, max_floors; float width_from, width_to; } LevelSettings[] = 
{
	// lvl       sides | decks | rooms | max_floors | width range
	/*  1 */ {     1,      1,      1,        3,         600,    900 },
	/*  2 */ {     2,      1,      2,        4,        1000,   1500 },
	/*  3 */ {     1,      2,      2,       99,         700,   1100 },
	/*  4 */ {     2,      2,      3,       99,         700,   1100 },
	/*  5 */ {     1,      1,      5,        3,         2000,  2600 },
	/*  6 */ {     2,      1,     10,        4,         2000,  3000 },
	/*  7 */ {     3,      1,      3,        3,         600,    900 },
	/*  8 */ {     2,      3,      4,        4,         600,    900 },
	/*  9 */ {     1,      1,      3,        4,         4000,  4300 },
	/* 10 */ {     3,      2,      3,        4,         600,    900 },
	/* 11 */ {     3,      2,      3,        4,         600,    900 },
	/* 12 */ {     1,      1,      5,       99,         600,    900 },
	/* 13 */ {     3,      1,      5,       99,         600,    900 },
	/* 14 */ {     3,      2,      5,       99,         600,    900 },
	/* 15 */ {     1,      3,      5,       99,        1000,   1500 },
};

static ZL_Vector linepos;
static ticks_t lineticks;
static const char* lastline;
static const char* lines[] =
{
"3 D PRINTER",
"ACTION FIGURE",
"AKIRA",
"ALCATRAZ",
"AMIGA",
"ANIME",
"AREA FIFTY ONE",
"ARMY OF DARKNESS",
"ASSEMBLY",
"ATARI",
"AURORA BOREALIS",
"AVATAR",
"AVENGERS",
"BABYLON 5",
"BACK TO THE FUTURE",
"BAKER STREET",
"BAT GIRL",
"BAT MAN",
"BATTLE STAR GALACTICA",
"BERMUDA TRIANGLE",
"BIGFOOT",
"BILL GATES",
"BLACK LOTUS",
"BLADE RUNNER",
"BOARD GAMES",
"BOOKS",
"BUFFY THE VAMPIRE SLAYER",
"C PLUS PLUS",
"C SHARP",
"CAPTAIN JAMES T KIRK",
"CAPTAIN PICARD",
"CARL SAGAN",
"CODING",
"COMIC CON",
"COMICS",
"COMMODORE",
"COSPLAY",
"CRACKING",
"CTHULU",
"CYBER PUNK",
"CYBER",
"DAFT PUNK",
"DARK SOULS",
"DARTH VADER",
"DARWIN",
"DE LOREAN",
"DEMO SCENE",
"DINOSAURS",
"DISC WORLD",
"DISNEY",
"DOCTOR WHO",
"DOS",
"DRACULA",
"DRAGON BALLS",
"DREAM CAST",
"DUNE",
"DUNGEONS AND DRAGONS",
"DWARF FORTRESS",
"E SPORTS",
"ELDER SCROLLS",
"ELVES",
"ENTERPRISE",
"ESCAPE ROOMS",
"FIRE FLY",
"FORTRESS OF SOLITUDE",
"FREE SOFTWARE",
"GAME BOY",
"GAME CUBE",
"GAME MASTER",
"GAME OF THRONES",
"GAMING",
"GANDALF",
"GEEKS",
"GEO CACHE",
"GEORGE LUCAS",
"GHOST BUSTERS",
"GHOST WALK",
"GO",
"GOTHAM",
"H T M L",
"HACKER",
"HACKERS",
"HALLOWEEN",
"HARRY POTTER",
"HAUNTED HOUSE",
"HITCH HIKERS GUIDE",
"HOBBITS",
"HOGWARTS",
"HOVER BOARD",
"HULK",
"INTERNET",
"IRON MAN",
"J POP",
"JACK THE RIPPER",
"JAIL BREAK",
"JAPAN",
"JAVA SCRIPT",
"JEDI KNIGHTS",
"JOHN WICK",
"JOKER",
"JULES VERNE",
"JURASSIC PARK",
"K POP",
"KINGS LANDING",
"KLINGON",
"LAN PARTY",
"LARGE HADRON COLLIDER",
"LASERS",
"LEET",
"LEGO",
"LIGHT SABER",
"LINUX",
"LOCH NESS",
"LOCK PICKS",
"LORD OF THE RINGS",
"LOST",
"LUA",
"LUKE SKYWALKER",
"MAGIC THE GATHERING",
"MAKERS",
"MANDALORIAN",
"MANGA",
"MARTY MAC FLY",
"MARVEL",
"MATRIX",
"METROPOLIS",
"MIDDLE EARTH",
"MINE CRAFT",
"MONTY PYTHON",
"MORDOR",
"MOVIES",
"MURDER MYSTERY",
"N C C 1 7 0 1",
"NARUTO",
"NASA",
"NERDS",
"NEURO MANCER",
"NINETEEN EIGHTYFOUR",
"NINTENDO",
"OBI WAN KENOBI",
"OTAKU",
"P H P",
"PAC MAN",
"PERFECT GRADE",
"PERL",
"PHASERS",
"PIRATES",
"PLAY STATION",
"POD CAST",
"POKEMON",
"POWER RANGERS",
"PROGRAMMING",
"PUZZLES",
"PYTHON",
"R 2 D 2",
"RASPBERRY PI",
"ROSWELL",
"RUBE GOLDBERG MACHINE",
"RUBIKS CUBE",
"RUST",
"SCIENCE FICTION",
"SEGA",
"SHAKESPEARE",
"SHERLOCK HOLMES",
"SHIPWRECKS",
"SNOW CRASH",
"SOCIAL MEDIA",
"SPACE BALLS",
"SPACE INVADERS",
"SPACE SHUTTLE",
"SPACE",
"SPEED RUNNING",
"SPIDER MAN",
"SPOCK",
"STAR GATE",
"STAR TREK",
"STAR WARS",
"STRANGER THINGS",
"SUPER MAN",
"SUPER MARIO",
"THE EXPANSE",
"TED TALKS",
"TETRIS",
"TIME TRAVEL",
"TOKYO",
"TRANS FORMERS",
"TREASURE HUNTING",
"TRON",
"TWIN PEAKS",
"UNIX",
"V R",
"VAPOR WAVE",
"VIDEO GAMES",
"WAR CRAFT",
"WAR HAMMER",
"WEIRD AL",
"WITCHES",
"WONDER WOMEN",
"WOOT",
"EX BOX",
"EX FILES",
"EX MAN",
"YODA",
"ZELDA",
"ZOMBIES",
};

struct sThing
{
	cpBody *body;
	enum eType { WALL, FLOOR, SUMO, NERD } type;
	ZL_Color color;
};

enum CollisionTypes { COLLISION_TOWER = 1, COLLISION_SUMO };

static std::vector<sThing> things;

static void AddThing(cpBody *body, sThing::eType type, ZL_Color color = ZLWHITE)
{
	things.push_back({body, type, color});
}

static void RemoveThing(size_t i, bool effect = true)
{
	sThing t = things[i];
	if (t.type == sThing::SUMO && effect)
	{
		particleSmoke.Spawn(200, t.body->p);
		sndHit.Play();
	}
	cpSpaceRemoveShape(space, t.body->shapeList);
	cpSpaceRemoveBody(space, t.body);
	cpShapeFree(t.body->shapeList);
	cpBodyFree(t.body);
	things.erase(things.begin() + i);

}

static void DrawTextBordered(const ZL_TextBuffer& buf, const ZL_Vector& p, scalar scale = 1, const ZL_Color& colfill = ZLWHITE, const ZL_Color& colborder = ZLBLACK, int border = 2, ZL_Origin::Type origin = ZL_Origin::Center)
{
	for (int i = 0; i < 9; i++) if (i != 4) buf.Draw(p.x+(border*((i%3)-1)), p.y+(border*((i/3)-1)), scale, scale, colborder, origin);
	buf.Draw(p.x, p.y, scale, scale, colfill, origin);
}

static void BuildLevel(int goto_level)
{
	while (things.size()) RemoveThing(things.size()-1, false);
	while (ground->shapeList)
	{
		cpShape* groundshape = ground->shapeList;
		cpSpaceRemoveShape(space, groundshape);
		cpShapeFree(groundshape);
	}

	{
		cpShape* groundshape = cpSpaceAddShape(space, cpBoxShapeNew2(ground, cpBBNew(-10000, -20, 10000, 0), 0));
		cpShapeSetFriction(groundshape, 100);
	}

	level = ZL_Math::Clamp(goto_level, 0, (int)COUNT_OF(LevelSettings) - 1);
	level_sides = LevelSettings[level].sides;
	level_decks = LevelSettings[level].decks;
	int room_per_deck = LevelSettings[level].rooms;
	int max_floors = LevelSettings[level].max_floors;
	float max_x_from = LevelSettings[level].width_from;
	float max_x_to = LevelSettings[level].width_to;

	total_sumos = 0;
	int numy = 3;
	float decky = 0;
	level_width = level_height = 0;
	for (int deck = 0; deck != level_decks; deck++, decky = level_height)
	{
		level_decky[deck] = decky;
		for (float towerflip = -1.0f; towerflip < 1.1f; towerflip += 2.0f)
		{
			if ((towerflip < 0 && !(level_sides & 2)) || (towerflip > 0 && !(level_sides & 1))) continue;

			if (deck)
			{
				ZL_Vector groundpos((200 + 5000) * towerflip, decky - 10);
				cpShape* groundshape = cpSpaceAddShape(space, cpBoxShapeNew2(ground, cpBBNew(groundpos.x - 5000, groundpos.y - 10, groundpos.x + 5000, groundpos.y + 10), 0));
				cpShapeSetFriction(groundshape, 100);
			}

			float min_x = 200.0f;
			float max_x = RAND_RANGE(max_x_from, max_x_to);
			if (max_x > level_width) level_width = max_x;

			float y = decky;
			for (float ymax = y + (max_floors - .9f) * 100.0f; y <= ymax; y += 100.0f)
			{
				max_x -= RAND_RANGE(0,50);
				bool lastroom = false;
				for (float roomn = 0.1f, x = max_x, roomw = RAND_RANGE(104, 200), nextroomw; ; x -= roomw, roomw = nextroomw, roomn++)
				{
					bool firstroom = !(int)roomn;
					roomw = ZL_Math::Min(roomw, x - min_x);
					if (firstroom && roomw < 102) goto towerDone;

					{
						cpBody *b = cpSpaceAddBody(space, cpBodyNew(13, cpMomentForBox(13, 20, 80)));
						cpShape* shape = cpSpaceAddShape(space, cpBoxShapeNew(b, 20, 80, 0));
						cpBodySetPosition(b, cpv(x * towerflip, y + 40));
						cpShapeSetFriction(shape, 100);
						cpShapeSetCollisionType(shape, COLLISION_TOWER);
						AddThing(b, sThing::WALL);
					}
					if (lastroom) { min_x = x; break; }

					nextroomw = RAND_RANGE(104, 200);
					lastroom = (x - roomw - nextroomw < min_x || (y == decky && (int)roomn + 1 == room_per_deck));
					float l = x - roomw - (lastroom ? 10 : 0), r = x + (firstroom ? 10 : 0);
					{
						cpBody *b = cpSpaceAddBody(space, cpBodyNew(13, cpMomentForBox(13, r-l, 20)));
						cpShape* shape = cpSpaceAddShape(space, cpBoxShapeNew(b, r-l, 20, 0));
						cpBodySetPosition(b, cpv((l + (r-l) / 2) * towerflip, y + 90));
						cpShapeSetFriction(shape, 100);
						cpShapeSetCollisionType(shape, COLLISION_TOWER);
						AddThing(b, sThing::FLOOR);
					}
					{
						cpBody *b = cpSpaceAddBody(space, cpBodyNew(13, cpMomentForBox(13, 60, 60)));
						cpShape* shape = cpSpaceAddShape(space, cpBoxShapeNew(b, 60, 60, 0));
						cpBodySetPosition(b, cpv((l + (r-l) / 2) * towerflip, y + 32));
						cpShapeSetFriction(shape, 100);
						cpShapeSetCollisionType(shape, COLLISION_SUMO);
						AddThing(b, sThing::SUMO, RAND_COLOR);
						total_sumos++;
					}
				}
			}
			towerDone:
			y+= 100.0f;
			if (y > level_height) level_height = y;
		}
	}
	remainTicks = 10000;
	ticksClear = ticksFailed = 0;
	remain_sumos = total_sumos;
	level_width = (level_width * 1.1f) * (level_sides == 3 ? 2 : 1);

	static const ZL_Color skyColsTop[] = { ZLRGBFF( 23, 79,193) , ZLRGBFF( 16, 50,138) , ZLRGBFF(240,181, 52) , ZLRGBFF( 14, 38, 80) , ZLRGBFF( 54, 66,140) , ZLRGBFF( 22, 44, 68) , ZLRGBFF(141,104,137) , ZLRGBFF( 53, 57, 67) , ZLRGBFF(  9, 14, 39) , ZLRGBFF( 95,118,130) };
	static const ZL_Color skyColsBot[] = { ZLRGBFF( 97,169,255) , ZLRGBFF(228,217,177) , ZLRGBFF(143, 61, 69) , ZLRGBFF(209,214,194) , ZLRGBFF(247,131, 62) , ZLRGBFF(232,192,109) , ZLRGBFF(252,170, 80) , ZLRGBFF(223,202,162) , ZLRGBFF(209,210,211) , ZLRGBFF(114, 59, 70) };

	if (level > 0)
	{
		colSkyTopTarget = RAND_ARRAYELEMENT(skyColsTop);
		colSkyBottomTarget = RAND_ARRAYELEMENT(skyColsBot);
	}
	else
	{
		colSkyTopTarget = colSkyTop = skyColsTop[0];
		colSkyBottomTarget = colSkyBottom = skyColsBot[0];
		CannonY = 150.0f;
	}
}

static void PostStepRemoveBody(cpSpace *space, cpBody* body, void* data)
{
	for (size_t i = things.size(); i--;) { if (things[i].body == body) { RemoveThing(i); return; } }
}

static cpBool CollisionTowerToSumo(cpArbiter *arb, cpSpace *space, cpDataPointer userData)
{
	CP_ARBITER_GET_BODIES(arb, bTower, bSumo);
	ZL_ASSERT(bTower->shapeList->type == COLLISION_TOWER && bSumo->shapeList->type == COLLISION_SUMO);
	if (bSumo->p.y - 30.0f > bTower->p.y) return cpTrue;
	cpSpaceAddPostStepCallback(space, (cpPostStepFunc)PostStepRemoveBody, bSumo, NULL);
	return cpTrue;
}

static void Init()
{
	fntMain = ZL_Font("Data/matchbox.ttf.zip", 52);
	txtBuf = ZL_TextBuffer(fntMain);
	srfGround = ZL_Surface("Data/ground.png").SetTextureRepeatMode(true);
	srfWood = ZL_Surface("Data/wood.png");
	srfMetal = ZL_Surface("Data/metal.png");
	srfCannon = ZL_Surface("Data/cannon.png").SetOrigin(ZL_Origin::Custom(.5f, .5f)).SetScale(2, 1);
	srfNerd = ZL_Surface("Data/nerd.png").SetOrigin(ZL_Origin::Center).SetScale(1.5f);
	srfNerdShirt = ZL_Surface("Data/nerdshirt.png").SetOrigin(ZL_Origin::Center).SetScale(1.5f);
	srfSumo = ZL_Surface("Data/sumo.png").SetOrigin(ZL_Origin::Center).SetScale(2.0f);
	srfSumoPants = ZL_Surface("Data/sumopants.png").SetOrigin(ZL_Origin::Center).SetScale(2.0f);

	particleSmoke = ZL_ParticleEffect(300, 150);
	particleSmoke.AddParticleImage(ZL_Surface("Data/smoke.png").SetColor(ZLLUM(.5)), 1000);
	particleSmoke.AddBehavior(new ZL_ParticleBehavior_LinearMove(60, 55));
	particleSmoke.AddBehavior(new ZL_ParticleBehavior_LinearImageProperties(.5f, 0, s(1.1), s(.5)));

	imcMusic.Play();
	sndCannon = ZL_SynthImcTrack::LoadAsSample(&imcDataIMCCANNON);
	sndHit = ZL_SynthImcTrack::LoadAsSample(&imcDataIMCHIT);
	sndClear = ZL_SynthImcTrack::LoadAsSample(&imcDataIMCCLEAR);
	sndFail = ZL_SynthImcTrack::LoadAsSample(&imcDataIMCFAIL);

	space = cpSpaceNew();
	cpSpaceSetGravity(space, cpv(0.0f, -98.7f*3));
	cpSpaceAddCollisionHandler(space, COLLISION_TOWER, COLLISION_SUMO)->beginFunc = CollisionTowerToSumo;

	ground = cpSpaceAddBody(space, cpBodyNewStatic());
}

static void Update()
{
	if (OnTitle) return;

	static ticks_t TICKSUM = 0;
	for (TICKSUM += ZLELAPSEDTICKS
		#ifdef ZILLALOG //DEBUG DRAW
		*(ZL_Display::KeyDown[ZLK_LCTRL] ? 10 : 1)
		#endif
		; TICKSUM > 16; TICKSUM -= 16)
	{
		cpSpaceStep(space, s(16.0/1000.0));
	}

	float remainvel = 0;
	remain_sumos = 0;
	for (size_t i = things.size(); i--;)
	{
		sThing t = things[i];

		if (t.type == sThing::SUMO)
		{
			if (sabs(t.body->a) > .4f || cpvlengthsq(t.body->v) > 5000)
				RemoveThing(i);
			else
				remain_sumos++;
		}
		if ((t.type == sThing::WALL || t.type == sThing::FLOOR || t.type == sThing::NERD) && sabs(t.body->p.x) < level_width + 500.0f && t.body->p.y > 0.0f)
			remainvel += cpvlengthsq(t.body->v);
	}

	if (!ticksClear && !ticksFailed)
	{
		remainTicks = ZL_Math::Max(0, remainTicks - (int)ZLELAPSEDTICKS);
		if (!remain_sumos)
		{
			ticksClear = ZLTICKS;
			sndClear.Play();
		}
		else if (!remainTicks && remainvel < 500.0f && !CannonRange)
		{
			ticksFailed = ZLTICKS;
			sndFail.Play();
		}
	}

	bool bPlaying = (!ticksClear && !ticksFailed && remainTicks);
	if (ZL_Input::Down() && bPlaying) CannonRange = 100.0f;
	else if (CannonRange && ZL_Input::Held() && remainTicks) CannonRange = ZL_Math::Clamp(CannonRange + ZLELAPSEDF(1000), 100.0f, 2500.0f);
	else if (CannonRange && ((ZL_Input::Up() && bPlaying) || !remainTicks))
	{
		cpBody *b = cpSpaceAddBody(space, cpBodyNew(100, cpMomentForBox(100, 18, 36)));
		cpShape* shape = cpSpaceAddShape(space, cpBoxShapeNew(b, 18, 36, 0));
		cpShapeSetFriction(shape, 100);
		cpShapeSetCollisionType(shape, COLLISION_TOWER);
		cpBodySetPosition(b, cpv(0.0f, CannonY));
		cpBodySetVelocity(b, ZLV2CPV(CannonVel));
		cpBodySetAngle(b, CannonVel.GetAngle()-PIHALF);
		AddThing(b, sThing::NERD, RAND_COLOR);
		sndCannon.Play();

		TSMTXLOCK();
		tinysam_reset(ts);
		lastline = RAND_ARRAYELEMENT(lines);
		tinysam_speak_english(ts, lastline);
		lineticks = ZLTICKS;
		TSMTXUNLOCK();

		CannonRange = 0;
	}
	else CannonRange = 0;

	if (!bPlaying && ZL_Input::Down())
	{
		if (ticksClear && ZLSINCE(ticksClear) > 250)
		{
			if (level == COUNT_OF(LevelSettings)-1)
			{
				OnTitle = true;
				imcMusic.SetSongVolume(60);
				titleswitchtick = ZLTICKS;
			}
			else
				BuildLevel(level + 1);
		}
		if (ticksFailed && ZLSINCE(ticksFailed) > 250) BuildLevel(level);
	}

	#ifdef ZILLALOG //DEBUG
	if (ZL_Input::Down(ZLK_F9)) BuildLevel(level - 1);
	if (ZL_Input::Down(ZLK_F10)) BuildLevel(level);
	if (ZL_Input::Down(ZLK_F11)) BuildLevel(level + 1);
	#endif

	if (ZL_Input::Up(ZLK_ESCAPE, true))
	{
		OnTitle = true;
		imcMusic.SetSongVolume(60);
	}

}

static void Draw()
{
	if (OnTitle)
	{
		static ZL_Color introcolor = RAND_COLOR*.2f, introcolortgt = introcolor;
		if ((ZLTICKS%1000) >= 500 && ((ZLTICKS-ZLELAPSEDTICKS)%1000) < 500) introcolortgt = RAND_COLOR*.2f;
		introcolor = ZL_Color::Lerp(introcolor, introcolortgt, 0.01f);
		ZL_Display::FillGradient(-10000, -5, 10000, ZL_Display::ScreenToWorld(0,ZLHEIGHT).y, introcolor, introcolor, introcolor, introcolor);

		ZL_SeededRand rnd((ZLTICKS/100)*999);
		float scale1 = rnd.Range(2.9f, 3.1f), scale2 = rnd.Range(2.9f, 3.1f);
		txtBuf.SetText(0.5f, ZL_String::format("ANGRY\nNERDS"));
		ZL_Display::PushMatrix();
		ZL_Display::Translate(ZLHALFW, ZLHALFH+100);
		ZL_Display::Rotate(rnd.Variation(.1f));
		txtBuf.Draw(rnd.AngleVec()*40, scale1, scale1, ZLLUMA(0, .5), ZL_Origin::Center);
		ZL_Display::PopMatrix();
		ZL_Display::PushMatrix();
		ZL_Display::Translate(ZLHALFW, ZLHALFH+100);
		ZL_Display::Rotate(rnd.Variation(.1f));
		DrawTextBordered(txtBuf, ZL_Vector::Zero, scale2, ZL_Color(introcolor.g*5, introcolor.b*5, introcolor.r*5), ZLBLACK, 2);
		ZL_Display::PopMatrix();

		srfSumo.Draw(150, ZLFROMH(300), 4, 4);
		srfSumo.Draw(ZLFROMW(150), ZLFROMH(300), -4, 4);

		static std::vector<ZL_Vector> nerds;
		if (nerds.size() < 10 && (ZLTICKS%1000) >= 500 && ((ZLTICKS-ZLELAPSEDTICKS)%1000) < 500)
			nerds.push_back(ZL_Vector(((nerds.size() & 1) ? ZLFROMW(150) : 150), ZLFROMH(300)) + RAND_ANGLEVEC * 800);

		for (size_t i = nerds.size(); i--;)
		{
			ZL_Vector tgt(((i & 1) ? ZLFROMW(150) : 150), ZLFROMH(300));
			nerds[i] = ZL_Vector::Lerp(nerds[i], tgt, .1f);
			ZL_Vector dlt = tgt - nerds[i];
			srfNerd.Draw(nerds[i], dlt.GetAngle()-PIHALF, 2, 2);
			if (dlt < 80.0f) nerds[i] = tgt + RAND_ANGLEVEC * 800;
		}

		static ZL_TextBuffer txtClickToPlay(fntMain, "CLICK TO START");
		static ZL_TextBuffer txtInfo(fntMain, 0.5f, "HOLD LEFT MOUSE BUTTON TO CHARGE THE NERD CANNON\nUSE W AND S KEYS OR RIGHT MOUSE BUTTON TO RAISE OR LOWER THE NERD CANNON");
		static ZL_TextBuffer txtFooter(fntMain, "MADE IN 2022 BY BERNHARD SCHELLING FOR LUDUM DARE 51");
		static ZL_TextBuffer txtFullScreen(fntMain, 0.5f, "CLICK HERE\nFULL SCREEN");

		DrawTextBordered(txtClickToPlay, ZLV(ZLHALFW, 200), 1.0f, ZL_Color::Yellow, ZLBLACK, 4);
		DrawTextBordered(txtInfo, ZLV(ZLHALFW, 120*.8f), .5f, ZLWHITE, ZLBLACK, 4);
		DrawTextBordered(txtFooter, ZLV(ZLHALFW, 25*.5f), .4f, ZLWHITE, ZLBLACK, 3);

		ZL_Rectf recFullScreen(ZLFROMW(300), ZLFROMH(140), ZLWIDTH-10, ZLHEIGHT-10);
		bool hoverFullScreen = !!ZL_Input::Hover(recFullScreen);
		ZL_Display::DrawRect(recFullScreen, ZLBLACK, ZLLUMA((hoverFullScreen ? .8 : .5), .5));
		DrawTextBordered(txtFullScreen, recFullScreen.Center(), .6f, ZLWHITE, ZLBLACK, 3);
		if (ZL_Input::Clicked(recFullScreen))
		{
			ZL_Display::ToggleFullscreen();
		}

		if (ZL_Input::ClickedOutside(recFullScreen) && ZLSINCE(titleswitchtick) > 500)
		{
			OnTitle = false;
			imcMusic.SetSongVolume(40);
			BuildLevel(0);
		}
		if (ZL_Input::Up(ZLK_ESCAPE, true) && ZLSINCE(titleswitchtick) > 500)
			ZL_Application::Quit();
		return;
	}

	// Calculate camera transform
	float targetCameraX = 0;
	if (level_sides & 1) targetCameraX -= ZLHALFW-100;
	if (level_sides & 2) targetCameraX += ZLHALFW-100;
	float targetCameraZoom = ZL_Math::Min(ZLWIDTH / (level_width + 100), ZLHEIGHT / level_height);
	CameraX = ZL_Math::Lerp(CameraX, targetCameraX, .1f);
	CameraZoom = ZL_Math::Lerp(CameraZoom, targetCameraZoom, .1f);

	// Transform camera
	ZL_Display::PushMatrix();
	ZL_Display::Translate(ZLHALFW + CameraX, 0);
	ZL_Display::Scale(CameraZoom);
	ZL_Display::Translate(0, 50);

	colSkyTop = ZL_Color::Lerp(colSkyTop, colSkyTopTarget, .1f);
	colSkyBottom = ZL_Color::Lerp(colSkyBottom, colSkyBottomTarget, .1f);
	ZL_Display::FillGradient(-10000, -5, 10000, ZL_Display::ScreenToWorld(0,ZLHEIGHT).y, colSkyTop, colSkyTop, colSkyBottom, colSkyBottom);

	// Calculate pointer position with transformed camera
	ZL_Vector pointerInWorld = ZL_Display::ScreenToWorld(ZL_Input::Pointer());
	float moveY = ZL_Math::Clamp1(((ZL_Input::Held(ZLK_W) || ZL_Input::Held(ZLK_UP)) ? 1.f : 0.f)
				+ ((ZL_Input::Held(ZLK_S) || ZL_Input::Held(ZLK_DOWN)) ? -1.f : 0.f)
				+ ((ZL_Input::Held(ZL_BUTTON_RIGHT) && sabs(pointerInWorld.y - CannonY) > 10 ) ? (pointerInWorld.y > CannonY ? 1.f : -1.f) : 0.f));
	if (moveY)
	{
		CannonY = ZL_Math::Max(50.0f, CannonY + moveY * ZLELAPSEDF(250));
	}

	// Draw Shadows
	for (sThing t : things)
	{
		#define THING_SHADOW(p) p.x + 5, p.y - 5
		if (t.type == sThing::FLOOR || t.type == sThing::WALL)
		{
			cpSplittingPlane *p = ((cpPolyShape *)t.body->shapeList)->planes;
			ZL_Display::FillQuad(THING_SHADOW(p[0].v0), THING_SHADOW(p[1].v0), THING_SHADOW(p[2].v0), THING_SHADOW(p[3].v0), ZLLUMA(0, 0.5));
		}
		else if (t.type == sThing::NERD)
			srfNerd.Draw(THING_SHADOW(t.body->p), t.body->a, ZLLUMA(0, 0.5));
		else if (t.type == sThing::SUMO)
			srfSumo.Draw(THING_SHADOW(t.body->p), t.body->a, (t.body->p.x > 0 ? -srfSumo.GetScaleW() : srfSumo.GetScaleW()), srfSumo.GetScaleH(), ZLLUMA(0, 0.5));
	}

	// Draw grass grounds
	srfGround.DrawTo(-10000, -64, 10000, 00);
	for (int deck = 1; deck < level_decks; deck++)
	{
		if (level_sides & 1) srfGround.DrawTo(200, level_decky[deck]-64, 10000, level_decky[deck]);
		if (level_sides & 2) srfGround.DrawTo(-10000, level_decky[deck]-64, -200, level_decky[deck]);
	}

	// Draw shoot line
	ZL_Vector cannonDir = (pointerInWorld - ZLV(0, CannonY)).Norm();
	if (cannonDir.y < .25f) { cannonDir.x = (cannonDir.x < 0 ? -1.0f : 1.0f); cannonDir.y = .25f; cannonDir.Norm(); }
	if (ZL_Input::Held() && CannonRange)
	{
		CannonVel = cannonDir * CannonRange;
		ZL_Display::DrawWideLine(ZLV(0, CannonY), ZLV(0, CannonY) + CannonVel.VecWithLength(50.0f+CannonRange*.1f), 5.0f, ZL_Color::White, ZL_Color::White);
	}
	if (ZL_Input::Up())
	{
		linepos = ZL_Display::WorldToScreen(ZLV(0, CannonY - 50));
		if (linepos.x > ZLFROMW(50)) linepos.x = ZLFROMW(50);
		if (linepos.x < 50) linepos.x = 50;
		if (linepos.y < 50) linepos.y = 50;
	}

	// Draw all things
	for (sThing t : things)
	{
		cpSplittingPlane *planes = ((cpPolyShape *)t.body->shapeList)->planes;
		if (t.type == sThing::WALL) srfWood.DrawQuad(planes[0].v0, planes[1].v0, planes[2].v0, planes[3].v0);
		if (t.type == sThing::FLOOR) srfMetal.DrawQuad(planes[0].v0, planes[1].v0, planes[2].v0, planes[3].v0);
		if (t.type == sThing::NERD)
		{
			srfNerd.Draw(t.body->p.x, t.body->p.y, t.body->a);
			srfNerdShirt.Draw(t.body->p.x, t.body->p.y, t.body->a, t.color);
		}
		if (t.type == sThing::SUMO)
		{
			srfSumo.Draw(t.body->p.x, t.body->p.y, t.body->a, (t.body->p.x > 0 ? -srfSumo.GetScaleW() : srfSumo.GetScaleW()), srfSumo.GetScaleH());
			srfSumoPants.Draw(t.body->p.x, t.body->p.y, t.body->a, (t.body->p.x > 0 ? -srfSumo.GetScaleW() : srfSumo.GetScaleW()), srfSumo.GetScaleH(), t.color);
		}
	}

	// Draw cannon base and cannon
	float throwAngle = cannonDir.GetAngle();
	ZL_Display::FillRect(-25.0f, 0.0f, 25.0f, CannonY, ZL_Color::Black);
	srfCannon.Draw(0.0f, CannonY, throwAngle, srfCannon.GetScaleW(), (cannonDir.x < 0 ? -srfCannon.GetScaleH() : srfCannon.GetScaleH()));

	#ifdef ZILLALOG //DEBUG DRAW
	if (ZL_Display::KeyDown[ZLK_LSHIFT])
	{
		ZL_Display::DrawLine(-10000, 0, 10000, 0, ZL_Color::Gray);
		ZL_Display::DrawLine(0, -10000, 0, 10000, ZL_Color::Gray);
		ZL_Display::DrawLine(-10000, level_height, 10000, level_height, ZL_Color::Gray);
		ZL_Display::DrawLine(level_width, -10000, level_width, 10000, ZL_Color::Gray);
		ZL_Display::DrawLine(-level_width, -10000, -level_width, 10000, ZL_Color::Gray);
		void DebugDrawShape(cpShape*,void*); cpSpaceEachShape(space, DebugDrawShape, NULL);
		void DebugDrawConstraint(cpConstraint*, void*); cpSpaceEachConstraint(space, DebugDrawConstraint, NULL);
	}
	#endif

	particleSmoke.Draw();

	ZL_Display::PopMatrix();

	if (lineticks && ZLSINCE(lineticks) < 1000)
	{
		txtBuf.SetText(lastline);
		DrawTextBordered(txtBuf, linepos, 0.5f, ZLWHITE, ZLBLACK, 2, (linepos.x < ZLHALFH/2 ? ZL_Origin::CenterLeft : (linepos.x > ZLHALFH*3/2 ? ZL_Origin::CenterRight : ZL_Origin::Center)));
	}

	txtBuf.SetText(0.5f, ZL_String::format("LEVEL\n%d", level+1));
	DrawTextBordered(txtBuf, ZLV(10, ZLFROMH(50)), 1, ZLWHITE, ZLBLACK, 2, ZL_Origin::TopLeft);
	txtBuf.SetText(0.5f, ZL_String::format("TIME\n%d", ZL_Math::Max(0, (int)((999+remainTicks)/1000))));
	DrawTextBordered(txtBuf, ZLV(ZLHALFW, ZLFROMH(50)), 1, ZLWHITE, ZLBLACK, 2, ZL_Origin::TopCenter);
	txtBuf.SetText(0.5f, ZL_String::format("REMAINING\n%d OF %d", remain_sumos, total_sumos));
	DrawTextBordered(txtBuf, ZLV(ZLFROMW(10), ZLFROMH(50)), 1, ZLWHITE, ZLBLACK, 2, ZL_Origin::TopRight);

	if (ticksClear)
	{
		if (level == COUNT_OF(LevelSettings)-1)
			txtBuf.SetText(0.5f, ZL_String::format("YOU FINISHED THE GAME!\n\nTHANKS FOR PLAYING!!\n\nCLICK TO GO BACK TO THE TITLE"));
		else
			txtBuf.SetText(0.5f, ZL_String::format("LEVEL CLEARED!\n\nCLICK TO CONTINUE"));
		DrawTextBordered(txtBuf, ZLV(ZLHALFW, ZLHALFH), 1.0f, ZL_Color::Green);
	}

	if (ticksFailed)
	{
		txtBuf.SetText(0.5f, ZL_String::format("LEVEL FAILED!\n\nCLICK TO RETRY"));
		DrawTextBordered(txtBuf, ZLV(ZLHALFW, ZLHALFH), 1.0f, ZL_Color::Orange);
	}
}

static struct sAngryNerds : public ZL_Application
{
	sAngryNerds() : ZL_Application(60) { }

	virtual void Load(int argc, char *argv[])
	{
		if (!ZL_Application::LoadReleaseDesktopDataBundle()) return;
		if (!ZL_Display::Init("Angry Nerds", 1280, 720, ZL_DISPLAY_ALLOWRESIZEHORIZONTAL)) return;
		ZL_Display::ClearFill(ZL_Color::White);
		ZL_Display::SetAA(true);
		ZL_Audio::Init();
		ZL_Input::Init();

		ts = tinysam_create();
		tinysam_set_output(ts, TINYSAM_STEREO_INTERLEAVED, 44100, .5f);
		tinysam_set_speed(ts, 100);
		tinysam_speak_english(ts, "Welcome to Angry Nerds");
		ZL_Audio::HookAudioMix([](short* buffer, unsigned int samples, bool need_mix)
		{
			TSMTXLOCK();
			bool res = !!tinysam_render_short(ts, buffer, samples, need_mix);
			TSMTXUNLOCK();
			return res;
		});

		::Init();
	}

	virtual void AfterFrame()
	{
		::Update();
		::Draw();
	}
} AngryNerds;

#ifdef ZILLALOG //DEBUG DRAW
void DebugDrawShape(cpShape *shape, void*)
{
	switch (shape->klass->type)
	{
		case CP_CIRCLE_SHAPE: {
			cpCircleShape *circle = (cpCircleShape *)shape;
			ZL_Display::DrawCircle(circle->tc, circle->r, ZL_Color::Green);
			break; }
		case CP_SEGMENT_SHAPE: {
			cpSegmentShape *seg = (cpSegmentShape *)shape;
			cpVect vw = cpvclamp(cpvperp(cpvsub(seg->tb, seg->ta)), seg->r);
			//ZL_Display::DrawLine(seg->ta, seg->tb, ZLWHITE);
			ZL_Display::DrawQuad(seg->ta.x + vw.x, seg->ta.y + vw.y, seg->tb.x + vw.x, seg->tb.y + vw.y, seg->tb.x - vw.x, seg->tb.y - vw.y, seg->ta.x - vw.x, seg->ta.y - vw.y, ZLRGBA(0,1,1,.35), ZLRGBA(1,1,0,.35));
			ZL_Display::DrawCircle(seg->ta, seg->r, ZLRGBA(0,1,1,.35), ZLRGBA(1,1,0,.35));
			ZL_Display::DrawCircle(seg->tb, seg->r, ZLRGBA(0,1,1,.35), ZLRGBA(1,1,0,.35));
			break; }
		case CP_POLY_SHAPE: {
			cpPolyShape *poly = (cpPolyShape *)shape;
			{for (int i = 1; i < poly->count; i++) ZL_Display::DrawLine(poly->planes[i-1].v0, poly->planes[i].v0, ZLWHITE);}
			ZL_Display::DrawLine(poly->planes[poly->count-1].v0, poly->planes[0].v0, ZLWHITE);
			break; }
	}
	ZL_Display::FillCircle(cpBodyGetPosition(shape->body), 3, ZL_Color::Red);
	ZL_Display::DrawLine(cpBodyGetPosition(shape->body), (ZL_Vector&)cpBodyGetPosition(shape->body) + ZLV(cpBodyGetAngularVelocity(shape->body)*-10, 0), ZLRGB(1,0,0));
	ZL_Display::DrawLine(cpBodyGetPosition(shape->body), (ZL_Vector&)cpBodyGetPosition(shape->body) + ZL_Vector::FromAngle(cpBodyGetAngle(shape->body))*10, ZLRGB(1,1,0));
}

void DebugDrawConstraint(cpConstraint *constraint, void *data)
{
	cpBody *body_a = constraint->a, *body_b = constraint->b;

	if(cpConstraintIsPinJoint(constraint))
	{
		cpPinJoint *joint = (cpPinJoint *)constraint;
		cpVect a = (cpBodyGetType(body_a) == CP_BODY_TYPE_KINEMATIC ? body_a->p : cpTransformPoint(body_a->transform, joint->anchorA));
		cpVect b = (cpBodyGetType(body_b) == CP_BODY_TYPE_KINEMATIC ? body_b->p : cpTransformPoint(body_b->transform, joint->anchorB));
		ZL_Display::DrawLine(a.x, a.y, b.x, b.y, ZL_Color::Magenta);
	}
	else if (cpConstraintIsPivotJoint(constraint))
	{
		cpPivotJoint *joint = (cpPivotJoint *)constraint;
		cpVect a = (cpBodyGetType(body_a) == CP_BODY_TYPE_KINEMATIC ? body_a->p : cpTransformPoint(body_a->transform, joint->anchorA));
		cpVect b = (cpBodyGetType(body_b) == CP_BODY_TYPE_KINEMATIC ? body_b->p : cpTransformPoint(body_b->transform, joint->anchorB));
		ZL_Display::DrawLine(a.x, a.y, b.x, b.y, ZL_Color::Magenta);
		ZL_Display::FillCircle(a, 2, ZL_Color::Magenta);
		ZL_Display::FillCircle(b, 2, ZL_Color::Magenta);
	}
	else if (cpConstraintIsRotaryLimitJoint(constraint))
	{
		cpRotaryLimitJoint *joint = (cpRotaryLimitJoint *)constraint;
		cpVect a = cpTransformPoint(body_a->transform, cpvzero);
		cpVect b = cpvadd(a, cpvmult(cpvforangle(joint->min), 40));
		cpVect c = cpvadd(a, cpvmult(cpvforangle(joint->max), 40));
		ZL_Display::DrawLine(a.x, a.y, b.x, b.y, ZL_Color::Magenta);
		ZL_Display::DrawLine(a.x, a.y, c.x, c.y, ZL_Color::Magenta);
	}
}
#endif

static const unsigned int IMCCANNON_OrderTable[] = {
	0x000000011,
};
static const unsigned char IMCCANNON_PatternData[] = {
	0x43, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x43, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const unsigned char IMCCANNON_PatternLookupTable[] = { 0, 1, 2, 2, 2, 2, 2, 2, };
static const TImcSongEnvelope IMCCANNON_EnvList[] = {
	{ 0, 386, 16, 8, 16, 255, true, 255, },
	{ 0, 256, 33, 8, 16, 255, true, 255, },
	{ 128, 256, 99, 8, 16, 255, true, 255, },
	{ 0, 128, 50, 8, 16, 255, true, 255, },
	{ 0, 256, 201, 5, 19, 255, true, 255, },
	{ 0, 256, 133, 8, 16, 255, true, 255, },
	{ 0, 256, 66, 8, 16, 255, true, 255, },
	{ 0, 256, 228, 8, 16, 255, true, 255, },
	{ 0, 256, 444, 8, 16, 255, true, 255, },
	{ 0, 256, 627, 23, 15, 255, true, 255, },
	{ 0, 256, 174, 8, 16, 255, true, 255, },
	{ 256, 271, 65, 8, 16, 255, true, 255, },
	{ 0, 512, 11073, 0, 255, 255, true, 255, },
};
static TImcSongEnvelopeCounter IMCCANNON_EnvCounterList[] = {
	{ 0, 0, 386 }, { 1, 0, 256 }, { 2, 0, 256 }, { 3, 0, 128 },
	{ -1, -1, 258 }, { 4, 0, 238 }, { -1, -1, 256 }, { 5, 0, 256 },
	{ 6, 1, 256 }, { 7, 1, 256 }, { 8, 1, 256 }, { -1, -1, 384 },
	{ 9, 1, 0 }, { 10, 1, 256 }, { 11, 1, 271 }, { 12, 1, 256 },
};
static const TImcSongOscillator IMCCANNON_OscillatorList[] = {
	{ 4, 227, IMCSONGOSCTYPE_SINE, 0, -1, 255, 1, 2 },
	{ 9, 15, IMCSONGOSCTYPE_NOISE, 0, -1, 255, 3, 4 },
	{ 4, 150, IMCSONGOSCTYPE_SINE, 0, -1, 255, 5, 6 },
	{ 5, 174, IMCSONGOSCTYPE_SINE, 0, -1, 230, 7, 6 },
	{ 6, 238, IMCSONGOSCTYPE_SINE, 1, -1, 0, 9, 6 },
	{ 5, 66, IMCSONGOSCTYPE_SINE, 1, -1, 134, 10, 11 },
	{ 7, 127, IMCSONGOSCTYPE_NOISE, 1, -1, 0, 12, 6 },
	{ 8, 0, IMCSONGOSCTYPE_NOISE, 1, -1, 0, 6, 6 },
	{ 6, 106, IMCSONGOSCTYPE_SINE, 1, -1, 142, 13, 14 },
	{ 5, 200, IMCSONGOSCTYPE_SAW, 1, -1, 104, 6, 6 },
	{ 8, 0, IMCSONGOSCTYPE_NOISE, 1, 5, 212, 6, 6 },
	{ 7, 0, IMCSONGOSCTYPE_NOISE, 1, 9, 228, 6, 15 },
};
static const TImcSongEffect IMCCANNON_EffectList[] = {
	{ 227, 0, 1, 0, IMCSONGEFFECTTYPE_LOWPASS, 6, 0 },
	{ 103, 218, 1, 0, IMCSONGEFFECTTYPE_RESONANCE, 6, 6 },
	{ 13716, 109, 1, 0, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 6 },
	{ 97, 206, 1, 1, IMCSONGEFFECTTYPE_RESONANCE, 6, 6 },
	{ 154, 0, 1, 1, IMCSONGEFFECTTYPE_LOWPASS, 6, 0 },
};
static unsigned char IMCCANNON_ChannelVol[8] = { 69, 110, 100, 110, 100, 100, 69, 110 };
static const unsigned char IMCCANNON_ChannelEnvCounter[8] = { 0, 8, 0, 0, 0, 0, 0, 0 };
static const bool IMCCANNON_ChannelStopNote[8] = { true, true, false, false, false, false, false, false };
TImcSongData imcDataIMCCANNON = {
	/*LEN*/ 0x1, /*ROWLENSAMPLES*/ 3575, /*ENVLISTSIZE*/ 13, /*ENVCOUNTERLISTSIZE*/ 16, /*OSCLISTSIZE*/ 12, /*EFFECTLISTSIZE*/ 5, /*VOL*/ 100,
	IMCCANNON_OrderTable, IMCCANNON_PatternData, IMCCANNON_PatternLookupTable, IMCCANNON_EnvList, IMCCANNON_EnvCounterList, IMCCANNON_OscillatorList, IMCCANNON_EffectList,
	IMCCANNON_ChannelVol, IMCCANNON_ChannelEnvCounter, IMCCANNON_ChannelStopNote };

static const unsigned int IMCHIT_OrderTable[] = {
	0x000000001,
};
static const unsigned char IMCHIT_PatternData[] = {
	0x50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const unsigned char IMCHIT_PatternLookupTable[] = { 0, 1, 1, 1, 1, 1, 1, 1, };
static const TImcSongEnvelope IMCHIT_EnvList[] = {
	{ 0, 386, 65, 8, 16, 255, true, 255, },
	{ 0, 256, 174, 8, 16, 255, true, 255, },
	{ 128, 256, 173, 8, 16, 255, true, 255, },
	{ 0, 128, 2615, 8, 16, 255, true, 255, },
	{ 0, 256, 348, 5, 19, 255, true, 255, },
	{ 0, 256, 418, 8, 16, 255, true, 255, },
};
static TImcSongEnvelopeCounter IMCHIT_EnvCounterList[] = {
	{ 0, 0, 386 }, { 1, 0, 256 }, { 2, 0, 256 }, { 3, 0, 128 },
	{ -1, -1, 258 }, { 4, 0, 238 }, { -1, -1, 256 }, { 5, 0, 256 },
};
static const TImcSongOscillator IMCHIT_OscillatorList[] = {
	{ 5, 150, IMCSONGOSCTYPE_SINE, 0, -1, 255, 1, 2 },
	{ 9, 15, IMCSONGOSCTYPE_NOISE, 0, -1, 255, 3, 4 },
	{ 5, 200, IMCSONGOSCTYPE_SINE, 0, -1, 170, 5, 6 },
	{ 5, 174, IMCSONGOSCTYPE_SINE, 0, -1, 230, 7, 6 },
};
static const TImcSongEffect IMCHIT_EffectList[] = {
	{ 113, 0, 1, 0, IMCSONGEFFECTTYPE_LOWPASS, 6, 0 },
	{ 220, 168, 1, 0, IMCSONGEFFECTTYPE_RESONANCE, 6, 6 },
};
static unsigned char IMCHIT_ChannelVol[8] = { 148, 128, 100, 100, 100, 100, 100, 100 };
static const unsigned char IMCHIT_ChannelEnvCounter[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const bool IMCHIT_ChannelStopNote[8] = { true, true, false, false, false, false, false, false };
TImcSongData imcDataIMCHIT = {
	/*LEN*/ 0x1, /*ROWLENSAMPLES*/ 2594, /*ENVLISTSIZE*/ 6, /*ENVCOUNTERLISTSIZE*/ 8, /*OSCLISTSIZE*/ 4, /*EFFECTLISTSIZE*/ 2, /*VOL*/ 100,
	IMCHIT_OrderTable, IMCHIT_PatternData, IMCHIT_PatternLookupTable, IMCHIT_EnvList, IMCHIT_EnvCounterList, IMCHIT_OscillatorList, IMCHIT_EffectList,
	IMCHIT_ChannelVol, IMCHIT_ChannelEnvCounter, IMCHIT_ChannelStopNote };

static const unsigned int IMCCLEAR_OrderTable[] = {
	0x000000001,
};
static const unsigned char IMCCLEAR_PatternData[] = {
	0x50, 0x52, 0x54, 0x55, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const unsigned char IMCCLEAR_PatternLookupTable[] = { 0, 1, 1, 1, 1, 1, 1, 1, };
static const TImcSongEnvelope IMCCLEAR_EnvList[] = {
	{ 0, 256, 272, 25, 31, 255, true, 255, },
	{ 0, 256, 152, 8, 16, 255, true, 255, },
	{ 0, 256, 173, 8, 16, 255, true, 255, },
};
static TImcSongEnvelopeCounter IMCCLEAR_EnvCounterList[] = {
	{ 0, 0, 2 }, { -1, -1, 256 }, { 1, 0, 256 }, { 2, 0, 256 },
	{ 2, 0, 256 },
};
static const TImcSongOscillator IMCCLEAR_OscillatorList[] = {
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 100, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 66, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 24, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 88, 2, 1 },
	{ 10, 0, IMCSONGOSCTYPE_SQUARE, 0, -1, 62, 3, 1 },
	{ 9, 0, IMCSONGOSCTYPE_SQUARE, 0, -1, 34, 4, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, 1, 36, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_NOISE, 0, 3, 14, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 1, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 2, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 3, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 4, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 5, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 6, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 7, -1, 100, 0, 0 },
};
static const TImcSongEffect IMCCLEAR_EffectList[] = {
	{ 226, 173, 1, 0, IMCSONGEFFECTTYPE_RESONANCE, 1, 1 },
	{ 204, 0, 1, 0, IMCSONGEFFECTTYPE_LOWPASS, 1, 0 },
	{ 10795, 655, 1, 0, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 1 },
	{ 51, 0, 15876, 0, IMCSONGEFFECTTYPE_DELAY, 0, 0 },
};
static unsigned char IMCCLEAR_ChannelVol[8] = { 97, 100, 100, 100, 100, 100, 100, 100 };
static const unsigned char IMCCLEAR_ChannelEnvCounter[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const bool IMCCLEAR_ChannelStopNote[8] = { true, false, false, false, false, false, false, false };
TImcSongData imcDataIMCCLEAR = {
	/*LEN*/ 0x1, /*ROWLENSAMPLES*/ 5292, /*ENVLISTSIZE*/ 3, /*ENVCOUNTERLISTSIZE*/ 5, /*OSCLISTSIZE*/ 15, /*EFFECTLISTSIZE*/ 4, /*VOL*/ 100,
	IMCCLEAR_OrderTable, IMCCLEAR_PatternData, IMCCLEAR_PatternLookupTable, IMCCLEAR_EnvList, IMCCLEAR_EnvCounterList, IMCCLEAR_OscillatorList, IMCCLEAR_EffectList,
	IMCCLEAR_ChannelVol, IMCCLEAR_ChannelEnvCounter, IMCCLEAR_ChannelStopNote };

static const unsigned int IMCFAIL_OrderTable[] = {
	0x000000001,
};
static const unsigned char IMCFAIL_PatternData[] = {
	0x3B, 255, 0x37, 255, 0x30, 255, 0x29, 255, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const unsigned char IMCFAIL_PatternLookupTable[] = { 0, 1, 1, 1, 1, 1, 1, 1, };
static const TImcSongEnvelope IMCFAIL_EnvList[] = {
	{ 0, 256, 272, 25, 31, 255, true, 255, },
	{ 0, 256, 152, 8, 16, 255, true, 255, },
	{ 0, 256, 173, 8, 16, 255, true, 255, },
};
static TImcSongEnvelopeCounter IMCFAIL_EnvCounterList[] = {
	{ 0, 0, 2 }, { -1, -1, 256 }, { 1, 0, 256 }, { 2, 0, 256 },
	{ 2, 0, 256 },
};
static const TImcSongOscillator IMCFAIL_OscillatorList[] = {
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 100, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 66, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 24, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 88, 2, 1 },
	{ 10, 0, IMCSONGOSCTYPE_SQUARE, 0, -1, 62, 3, 1 },
	{ 9, 0, IMCSONGOSCTYPE_SQUARE, 0, -1, 34, 4, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, 1, 36, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_NOISE, 0, 3, 14, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 1, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 2, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 3, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 4, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 5, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 6, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 7, -1, 100, 0, 0 },
};
static const TImcSongEffect IMCFAIL_EffectList[] = {
	{ 226, 173, 1, 0, IMCSONGEFFECTTYPE_RESONANCE, 1, 1 },
	{ 204, 0, 1, 0, IMCSONGEFFECTTYPE_LOWPASS, 1, 0 },
	{ 10795, 655, 1, 0, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 1 },
	{ 51, 0, 19845, 0, IMCSONGEFFECTTYPE_DELAY, 0, 0 },
};
static unsigned char IMCFAIL_ChannelVol[8] = { 97, 100, 100, 100, 100, 100, 100, 100 };
static const unsigned char IMCFAIL_ChannelEnvCounter[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const bool IMCFAIL_ChannelStopNote[8] = { true, false, false, false, false, false, false, false };
TImcSongData imcDataIMCFAIL = {
	/*LEN*/ 0x1, /*ROWLENSAMPLES*/ 6615, /*ENVLISTSIZE*/ 3, /*ENVCOUNTERLISTSIZE*/ 5, /*OSCLISTSIZE*/ 15, /*EFFECTLISTSIZE*/ 4, /*VOL*/ 100,
	IMCFAIL_OrderTable, IMCFAIL_PatternData, IMCFAIL_PatternLookupTable, IMCFAIL_EnvList, IMCFAIL_EnvCounterList, IMCFAIL_OscillatorList, IMCFAIL_EffectList,
	IMCFAIL_ChannelVol, IMCFAIL_ChannelEnvCounter, IMCFAIL_ChannelStopNote };

static const unsigned int IMCMUSIC_OrderTable[] = {
	0x022000121, 0x011000000, 0x022000232, 0x011000000, 0x011000363, 0x011000363, 0x011000363, 0x011000444,
	0x011000003, 0x011000664, 0x011000340, 0x010000444, 0x011000001, 0x021000014, 0x022000140, 0x022000400,
};
static const unsigned char IMCMUSIC_PatternData[] = {
	0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0x3B, 0x39,
	0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0x44, 0x45,
	0x40, 0x40, 0x44, 0, 0x42, 0x40, 0x42, 0, 0x40, 0x3B, 0x40, 0, 0x3B, 0x39, 0x3B, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255,
	0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0, 0,
	0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0x3B, 0x39,
	0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0x44, 0x45,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x40, 0x40, 0x44, 0, 0x42, 0x40, 0x42, 0, 0x40, 0x3B, 0x40, 0, 0x3B, 0x39, 0x3B, 0,
	0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0x3B, 0x39,
	0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0x44, 0x45,
	0x40, 0x40, 0x44, 0, 0x42, 0x40, 0x42, 0, 0x40, 0x3B, 0x40, 0, 0x3B, 0x39, 0x3B, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x40, 0, 0, 0, 0x49, 0, 0, 0, 0x49, 0, 0, 0, 0x49, 0, 0, 0,
	0x40, 0, 0x40, 0, 0x40, 0, 0x40, 0, 0x40, 0, 0x40, 0, 0x40, 0, 0x40, 0,
	0x40, 0, 0x40, 0, 0x40, 0, 0x40, 0, 0x40, 0, 0x40, 0, 0x40, 0, 0x40, 0,
	0x50, 0, 0, 0, 0x50, 0, 0, 0, 0x50, 0, 0, 0, 0x52, 0, 0, 0,
};
static const unsigned char IMCMUSIC_PatternLookupTable[] = { 0, 4, 10, 16, 16, 16, 16, 18, };
static const TImcSongEnvelope IMCMUSIC_EnvList[] = {
	{ 0, 256, 97, 8, 16, 0, true, 255, },
	{ 0, 256, 379, 8, 15, 255, true, 255, },
	{ 0, 256, 65, 8, 16, 4, true, 255, },
	{ 0, 256, 523, 1, 23, 255, true, 255, },
	{ 128, 256, 174, 8, 16, 16, true, 255, },
	{ 0, 256, 871, 8, 16, 16, true, 255, },
	{ 0, 256, 523, 8, 16, 255, true, 255, },
	{ 0, 256, 43, 8, 16, 255, true, 255, },
};
static TImcSongEnvelopeCounter IMCMUSIC_EnvCounterList[] = {
	{ 0, 0, 256 }, { -1, -1, 256 }, { 1, 0, 254 }, { 2, 1, 256 },
	{ 2, 2, 256 }, { 3, 6, 158 }, { 4, 6, 256 }, { 5, 6, 256 },
	{ 6, 6, 256 }, { 7, 7, 256 },
};
static const TImcSongOscillator IMCMUSIC_OscillatorList[] = {
	{ 8, 0, IMCSONGOSCTYPE_SAW, 0, -1, 168, 1, 1 },
	{ 10, 0, IMCSONGOSCTYPE_SAW, 0, -1, 96, 1, 1 },
	{ 7, 127, IMCSONGOSCTYPE_SAW, 0, -1, 158, 2, 1 },
	{ 6, 0, IMCSONGOSCTYPE_SINE, 0, 0, 16, 1, 1 },
	{ 6, 0, IMCSONGOSCTYPE_SINE, 0, 1, 50, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, 2, 100, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SQUARE, 1, -1, 100, 1, 1 },
	{ 7, 244, IMCSONGOSCTYPE_SQUARE, 1, -1, 100, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SAW, 1, 6, 20, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SAW, 1, 7, 20, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SQUARE, 2, -1, 100, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SAW, 2, 10, 20, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 3, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 4, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 5, -1, 100, 0, 0 },
	{ 5, 15, IMCSONGOSCTYPE_SINE, 6, -1, 72, 1, 6 },
	{ 8, 0, IMCSONGOSCTYPE_NOISE, 6, -1, 204, 7, 1 },
	{ 5, 227, IMCSONGOSCTYPE_SINE, 6, -1, 126, 8, 1 },
	{ 9, 0, IMCSONGOSCTYPE_SAW, 7, -1, 122, 1, 1 },
	{ 9, 15, IMCSONGOSCTYPE_SAW, 7, -1, 100, 1, 1 },
	{ 9, 85, IMCSONGOSCTYPE_SAW, 7, -1, 255, 1, 1 },
	{ 9, 106, IMCSONGOSCTYPE_SQUARE, 7, 18, 255, 1, 1 },
	{ 10, 48, IMCSONGOSCTYPE_SQUARE, 7, 19, 202, 1, 1 },
	{ 10, 48, IMCSONGOSCTYPE_SQUARE, 7, 20, 255, 1, 1 },
};
static const TImcSongEffect IMCMUSIC_EffectList[] = {
	{ 70, 49, 1, 0, IMCSONGEFFECTTYPE_RESONANCE, 1, 1 },
	{ 11303, 666, 1, 2, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 1 },
	{ 123, 86, 1, 2, IMCSONGEFFECTTYPE_RESONANCE, 1, 1 },
	{ 2286, 3669, 1, 6, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 1 },
	{ 76, 0, 1, 6, IMCSONGEFFECTTYPE_LOWPASS, 1, 0 },
	{ 191, 2, 1, 7, IMCSONGEFFECTTYPE_RESONANCE, 1, 1 },
};
static unsigned char IMCMUSIC_ChannelVol[8] = { 145, 110, 100, 100, 100, 100, 112, 53 };
static const unsigned char IMCMUSIC_ChannelEnvCounter[8] = { 0, 3, 4, 0, 0, 0, 5, 9 };
static const bool IMCMUSIC_ChannelStopNote[8] = { false, false, false, false, false, false, true, true };
TImcSongData imcDataIMCMUSIC = {
	/*LEN*/ 0x10, /*ROWLENSAMPLES*/ 5512, /*ENVLISTSIZE*/ 8, /*ENVCOUNTERLISTSIZE*/ 10, /*OSCLISTSIZE*/ 24, /*EFFECTLISTSIZE*/ 6, /*VOL*/ 60,
	IMCMUSIC_OrderTable, IMCMUSIC_PatternData, IMCMUSIC_PatternLookupTable, IMCMUSIC_EnvList, IMCMUSIC_EnvCounterList, IMCMUSIC_OscillatorList, IMCMUSIC_EffectList,
	IMCMUSIC_ChannelVol, IMCMUSIC_ChannelEnvCounter, IMCMUSIC_ChannelStopNote };
ZL_SynthImcTrack imcMusic(&imcDataIMCMUSIC);
