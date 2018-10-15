#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>

#include <switch.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>


#define TILE_SIZEX 48
#define TILE_SIZEY 48

#define CACHER 9
#define FLAG 10
#define BOMBE 11

#define SAVEPATH "sdmc:/switch/Minesweeper_NX/"
#define SAVEFILE "mineswpr.dat"

char filename[41]; // strlen(SAVEPATH) + strlen(SAVEFILE) + 1 = 41
const char* savepath;

SDL_Window * 	window;
SDL_Renderer * 	renderer;
SDL_Surface *	surface;

u8 CASE_X, CASE_Y;
u8 TILE_X, TILE_Y;

touchPosition Stylus;

u32 kDown;
u32 kHeld;
u32 kUp;

bool Timer;
bool game_over;
bool mode_debug;
bool mode_title;
bool mode_game;
bool mode_custom;
bool game;
bool anim;
bool custom_wait_touch;
bool data_changed;
u16 timing;
s16 chrono;
u16 highscore[3];
u8 customX;
u8 customY;
int customM;

u8 MAX_TILEX;//19
u8 MAX_TILEY;//14

u16 level_courant[19*14];
u16 level_courant_cache[19*14];

int nbr_mines;
int total_mines;

u16 mines_restante;
u16 cases_ouvertes;

u8 colonnes, lignes;

u8 frame;

u8 selector;
u8 saved_selector;
int newselector;

int tempsPrecedent;
int tempsActuel;

Mix_Chunk *son1;
Mix_Chunk *son2;

typedef struct 
{
	SDL_Texture * texture;
	SDL_Rect SrcR;
	SDL_Rect DestR;
} 
images;
images tile, background[5], sprite[2], smiley, chiffre, number;

void renderTexture(SDL_Texture *tex, SDL_Renderer *ren, int Srcx, int Srcy, int Destx, int Desty, int w, int h)
{
	SDL_Rect srce;
	srce.x = Srcx;
	srce.y = Srcy;
	srce.w = w;
	srce.h = h;

	SDL_Rect dest;
	dest.x = Destx;
	dest.y = Desty;
	dest.w = w;
	dest.h = h;

	SDL_RenderCopy(ren, tex, &srce, &dest);
}

void draw_rectangle(int positionX, int positionY, int tailleX, int tailleY)
{
    SDL_Rect Pos ;
    Pos.x = positionX;
    Pos.y = positionY;
    Pos.w = tailleX;
    Pos.h = tailleY;
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &Pos);
}

//TOUCH
bool inBox(touchPosition touch, int x1, int y1, int x2, int y2)
{
	int tx=touch.px;
	int ty=touch.py;

	if (tx > x1 && tx < x2 && ty > y1 && ty < y2)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool HeldtouchInBox(touchPosition touch, int x1, int y1, int x2, int y2)
{
	int tx=touch.px;
	int ty=touch.py;

	if (kHeld & KEY_TOUCH && tx > x1 && tx < x2 && ty > y1 && ty < y2)
	{
		return true;
	}
	else
	{
		return false;
	}
}

//From thomasychen/minesweeper Github///////////////////////////////////////////////
void rippleUncover(int rowClicked, int columnClicked)
{
	// On n'ouvre pas les flags, ni les mines.
	if ((level_courant[rowClicked*MAX_TILEY + columnClicked] == BOMBE) ||//Mine
		(level_courant_cache[rowClicked*MAX_TILEY + columnClicked] == FLAG) ||//Flag
		(level_courant_cache[rowClicked*MAX_TILEY + columnClicked] == 16))//already revealed
	{
		return;
	}

	// on ouvre la case
	level_courant_cache[rowClicked*MAX_TILEY + columnClicked] = 16;//Vide
	cases_ouvertes++;

	// Si il y a des mines autour, on stop
	if (level_courant[rowClicked*MAX_TILEY + columnClicked] != 0 )
	{
		return;
	}

	int row, column;

	for (row = 0; row < 3; row++)
	{
		for (column = 0; column < 3; column++)
		{
			if ((level_courant_cache[(rowClicked + row - 1)*MAX_TILEY + columnClicked + column - 1] == CACHER)//Zéro
				&& (rowClicked + row - 1 >= 0) && (columnClicked + column - 1 >= 0)
				&& (rowClicked + row - 1 <= MAX_TILEX-1) && (columnClicked + column - 1 <= MAX_TILEY-1))
			{
				rippleUncover(rowClicked + row - 1, columnClicked + column - 1 );
			}
		}
	}

	return;
}

void uncoverGameOver() {
	//On ouvre toutes les bombes non ouverte
	for(u8 lignes = 0; lignes < MAX_TILEX; lignes++)
	{
		for(u8 colonnes = 0; colonnes < MAX_TILEY; colonnes++)
		{
			if ((level_courant[lignes*MAX_TILEY + colonnes] == BOMBE)
				&& (level_courant_cache[lignes*MAX_TILEY + colonnes] != FLAG))
				level_courant_cache[lignes*MAX_TILEY + colonnes] = 16;
			else if ((level_courant[lignes*MAX_TILEY + colonnes] != BOMBE)
				&& (level_courant_cache[lignes*MAX_TILEY + colonnes] == FLAG))
				level_courant_cache[lignes*MAX_TILEY + colonnes] = 13;
		}
	}
	frame = 4;
}


void uncoverNeighbours(int rowClicked, int columnClicked)
{
	// test whether the current cell is revealed
	if (level_courant_cache[rowClicked*MAX_TILEY + columnClicked] != 16 ||
		level_courant[rowClicked*MAX_TILEY + columnClicked] > 8)
	{
		return;
	}

	// test whether the number of flags on neighbours matches the current cell's value
	u8 flags = 0;

	for (u8 i=(rowClicked>0?rowClicked-1:0); i <= rowClicked+1 && i<MAX_TILEX; i++)
	{
		for (u8 j=(columnClicked>0?columnClicked-1:0); j <= columnClicked+1 && j<MAX_TILEY; j++)
		{
			if (level_courant_cache[i*MAX_TILEY + j] == FLAG)
			{
				flags++;
			}
		}
	}
	if (flags != level_courant[rowClicked*MAX_TILEY + columnClicked])
	{
		return;
	}

	// open neighbour cells
	for (u8 i=(rowClicked>0?rowClicked-1:0); i <= rowClicked+1 && i<MAX_TILEX; i++)
	{
		for (u8 j=(columnClicked>0?columnClicked-1:0); j <= columnClicked+1 && j<MAX_TILEY; j++)
		{
			if (level_courant_cache[i*MAX_TILEY + j] == 16 ||
				level_courant_cache[i*MAX_TILEY + j] == FLAG)
			{
				continue;
			}
			if (level_courant[i*MAX_TILEY + j] == BOMBE)
			{
				// die
				game_over = true;
				game = false;

				//La bombe rouge
				level_courant[i*MAX_TILEY + j] = 12;//Mines rouge
				level_courant_cache[i*MAX_TILEY + j] = 16;//Vide
			}
			else 
			{
				rippleUncover(i, j);
			}
		}
	}
	if (game_over && !game)
	{
		uncoverGameOver();
	}
	return;
}

void Affiche_trois_chiffres(u16 valeur, u16 posx, u16 posy)
{
	u8 unite = 0;
	u8 dizaine = 0;
	u16 centaine = 0;

	if (valeur < 10)
	{
		unite = valeur;
		dizaine = 0;
		centaine = 0;
	}
	else if (valeur < 100)
	{
		centaine = 0;
		dizaine = (valeur / 10);
		unite = valeur - (dizaine * 10);
	}
	else
	{
		centaine = (valeur / 100);
		dizaine = (valeur - (centaine * 100)) / 10;
		unite = valeur - (centaine * 100) - (dizaine * 10);
	}

	renderTexture(chiffre.texture, renderer, centaine*78, 0, posx, posy, 78, 138);
	renderTexture(chiffre.texture, renderer, dizaine*78, 0, posx+78, posy, 78, 138);
	renderTexture(chiffre.texture, renderer, unite*78, 0, posx+156, posy, 78, 138);
}

void Affiche_nombre(u16 valeur, u16 posx, u16 posy)
{
	u8 unite;
	u8 dizaine;
	u16 centaine;

	if (valeur < 10)
	{
		unite = valeur;
		dizaine = 0;
		centaine = 0;

		renderTexture(number.texture, renderer, unite*24, 0, posx+36, posy, 24, 30);
	}
	else if (valeur < 100)
	{
		centaine = 0;
		dizaine = (valeur / 10);
		unite = valeur - (dizaine * 10);

		renderTexture(number.texture, renderer, dizaine*24, 0, posx+21, posy, 24, 30);
		renderTexture(number.texture, renderer, unite*24, 0, posx+51, posy, 24, 30);
	}
	else
	{
		centaine = (valeur / 100);
		dizaine = (valeur - (centaine * 100)) / 10;
		unite = valeur - (centaine * 100) - (dizaine * 10);

		renderTexture(number.texture, renderer, centaine*24, 0, posx+3, posy, 24, 30);
		renderTexture(number.texture, renderer, dizaine*24, 0, posx+33, posy, 24, 30);
		renderTexture(number.texture, renderer, unite*24, 0, posx+63, posy, 24, 30);
	}
}

void update_level_courant(u8 lignes, u8 colonnes) {
	if (level_courant[colonnes + lignes*MAX_TILEY] != BOMBE)
	{
		level_courant[colonnes + lignes*MAX_TILEY] = 0;

		for (u8 i=(lignes>0?lignes-1:0); i <= lignes+1 && i<MAX_TILEX; i++)
		{
			for (u8 j=(colonnes>0?colonnes-1:0); j <= colonnes+1 && j<MAX_TILEY; j++)
			{
				if (level_courant[j + i*MAX_TILEY] == BOMBE)
				{
					level_courant[colonnes + lignes*MAX_TILEY]++;
				}
			}
		}
	}
}

void Restart_Game()
{
	if (selector == 0)
	{
		//Easy
		total_mines = 8;
		MAX_TILEX = 9;
		MAX_TILEY = 9;
	}
	else if (selector == 1)
	{
		//Normal
		total_mines = 25;
		MAX_TILEX = 14;
		MAX_TILEY = 14;
	}
	else if (selector == 2)
	{
		//Hard
		total_mines = 60;
		MAX_TILEX = 19;
		MAX_TILEY = 14;
	}

	mines_restante = total_mines;
	chrono = 0;
	game = false;
	game_over = false;
	frame = 0;
	cases_ouvertes = 0;
	anim = false;
	CASE_X = (MAX_TILEX-1) / 2;
	CASE_Y = (MAX_TILEY-1) / 2;
	tempsPrecedent = 0;
	tempsActuel = 0;

	//On nettoi
	for(u8 lignes = 0; lignes < MAX_TILEX; lignes++)
	{
		for(u8 colonnes = 0; colonnes < MAX_TILEY; colonnes++)
		{
			level_courant[colonnes + lignes*MAX_TILEY] = 0;

			//On remplie le tableau pour cacher
			level_courant_cache[colonnes + lignes*MAX_TILEY] = CACHER;
		}
	}
}

void printGame()
{
	//CLEAR
	SDL_RenderClear(renderer);

	if (mode_title)
	{
		//On affiche le BG
		SDL_RenderCopy(renderer, background[0].texture, NULL, NULL);

		//On affiche le selecteur
		renderTexture(sprite[0].texture, renderer, 0, 0, 355, 96 + selector*144, 96, 96);
		renderTexture(sprite[0].texture, renderer, 0, 0, 829, 96 + selector*144, 96, 96);
	}
	else if (mode_game)
	{
		//On affiche le BG
		SDL_RenderCopy(renderer, background[2].texture, NULL, NULL);

		//Le smiley
		renderTexture(smiley.texture, renderer, frame*156, 0, 82, 360,  156, 156);

		//On affiche les compteurs mines et chrono & Hi-S
		Affiche_trois_chiffres(mines_restante, 43, 43);

		if (chrono == 0) Affiche_trois_chiffres(chrono, 43, 539);
		else Affiche_trois_chiffres(chrono-1, 43, 539);

		if (selector <= 2) Affiche_trois_chiffres(highscore[selector], 43, 200);

		//Un rectangle sur mesure pour cadrer la grille
		draw_rectangle(320+(960-(MAX_TILEX*TILE_SIZEX))/2-3, (720-(MAX_TILEY*TILE_SIZEY))/2-3, MAX_TILEX*TILE_SIZEX+6, MAX_TILEY*TILE_SIZEY+6);

		//On affiche l'une ou l'autre couches de tiles
		for(lignes = 0; lignes < MAX_TILEX; lignes++)
		{
			for(colonnes = 0; colonnes < MAX_TILEY; colonnes++)
			{
				if (level_courant_cache[lignes*MAX_TILEY + colonnes]==16)
				{
					//Les tiles, chiffres et bombes
					renderTexture(tile.texture, renderer,
						level_courant[lignes*MAX_TILEY + colonnes]*48, 0,
							320+(960-(MAX_TILEX*TILE_SIZEX))/2 + lignes*TILE_SIZEX, (720-(MAX_TILEY*TILE_SIZEY))/2 + colonnes*TILE_SIZEY,
								TILE_SIZEX, TILE_SIZEY);
				}
				else
				{
					//Les tiles du dessus
					renderTexture(tile.texture, renderer,
						level_courant_cache[lignes*MAX_TILEY + colonnes]*48, 0,
							320+(960-(MAX_TILEX*TILE_SIZEX))/2 + lignes*TILE_SIZEX, (720-(MAX_TILEY*TILE_SIZEY))/2 + colonnes*TILE_SIZEY,
								TILE_SIZEX, TILE_SIZEY);
				}
			}
		}

		//On affiche le curseur
		renderTexture(sprite[1].texture, renderer, 0, 0, 320+(960-(MAX_TILEX*TILE_SIZEX))/2 + CASE_X*TILE_SIZEX,  (720-(MAX_TILEY*TILE_SIZEY))/2 + CASE_Y*TILE_SIZEY, TILE_SIZEX, TILE_SIZEY);
	}
	else if (mode_custom)
	{
		//On affiche le BG
		SDL_RenderCopy(renderer, background[4].texture, NULL, NULL);

		SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);

		//Lignes
		Affiche_nombre(MAX_TILEY, 595, 144);
        SDL_RenderDrawLine(renderer, 460, 212, 1280-460, 212);
		SDL_RenderDrawLine(renderer, 460, 213, 1280-460, 213);
		SDL_RenderDrawLine(renderer, 460, 214, 1280-460, 214);
		SDL_RenderDrawLine(renderer, 460 + 30*(MAX_TILEY-2), 212-15, 460 + 30*(MAX_TILEY-2), 212+17);
		SDL_RenderDrawLine(renderer, 460 + 30*(MAX_TILEY-2), 213-15, 460 + 30*(MAX_TILEY-2), 213+17);
		SDL_RenderDrawLine(renderer, 460 + 30*(MAX_TILEY-2), 214-15, 460 + 30*(MAX_TILEY-2), 214+17);

		//Colonnes
		Affiche_nombre(MAX_TILEX, 595, 324);
		SDL_RenderDrawLine(renderer, 385, 392, 1280-385, 392);
		SDL_RenderDrawLine(renderer, 385, 393, 1280-385, 393);
		SDL_RenderDrawLine(renderer, 385, 394, 1280-385, 394);
		SDL_RenderDrawLine(renderer, 385 + 30*(MAX_TILEX-2), 392-15, 385 + 30*(MAX_TILEX-2), 392+17);
		SDL_RenderDrawLine(renderer, 385 + 30*(MAX_TILEX-2), 393-15, 385 + 30*(MAX_TILEX-2), 393+17);
		SDL_RenderDrawLine(renderer, 385 + 30*(MAX_TILEX-2), 394-15, 385 + 30*(MAX_TILEX-2), 394+17);

		if (total_mines > MAX_TILEX*MAX_TILEY-1) total_mines = MAX_TILEX*MAX_TILEY-1;

		//Les mines
		Affiche_nombre(total_mines, 595, 504);
		SDL_RenderDrawLine(renderer, 243, 572, 1280-242, 572);
		SDL_RenderDrawLine(renderer, 243, 573, 1280-242, 573);
		SDL_RenderDrawLine(renderer, 243, 574, 1280-242, 574);
		SDL_RenderDrawLine(renderer, 243 + 3*(total_mines-1), 572-15, 243 + 3*(total_mines-1), 572+17);
		SDL_RenderDrawLine(renderer, 243 + 3*(total_mines-1), 573-15, 243 + 3*(total_mines-1), 573+17);
		SDL_RenderDrawLine(renderer, 243 + 3*(total_mines-1), 574-15, 243 + 3*(total_mines-1), 574+17);
	}

	//REFRESH
    SDL_RenderPresent(renderer);
}


void manageInput()
{
	if (mode_title)
	{
		if (kDown & KEY_DOWN)
		{
			selector++;
			selector %= 4;
		}
		else if (kDown & KEY_UP)
		{
			if (selector > 0)
			{
				selector--;
			}
			else
			{
				selector = 3;
			}
		}
		else if ((kUp & KEY_A) && (selector < 3))
		{
			mode_title = false;
			mode_game = true;

			Restart_Game();
		}
		else if ((kUp & KEY_A) && (selector == 3))
		{
			mode_title = false;
			mode_custom = true;
			MAX_TILEX = customX;
			MAX_TILEY = customY;
			total_mines = customM;
			custom_wait_touch = true;
		}
		
		if (kDown & KEY_TOUCH)
		{
			u8 newselector;
			if (inBox(Stylus, 526, 108, 753, 182))//EASY
			{
				newselector = 0;
	
				if (selector == newselector)
				{
					mode_title = false;
					mode_game = true;

					Restart_Game();
				}
				else
				{
					selector = newselector;
				}
			}
			else if (inBox(Stylus, 460, 252, 819, 326))//NORMAL
			{
				newselector = 1;

				if (selector == newselector)
				{
					mode_title = false;
					mode_game = true;

					Restart_Game();
				}
				else
				{
					selector = newselector;
				}
			}
			else if (inBox(Stylus, 526, 396, 751, 468))//HARD
			{
				newselector = 2;

				if (selector == newselector)
				{
					mode_title = false;
					mode_game = true;

					Restart_Game();
				}
				else
				{
					selector = newselector;
				}
			}
			else if (inBox(Stylus, 460, 540, 822, 614))//CUSTOM
			{
				newselector = 3;

				if (selector == newselector)
				{
					mode_title = false;
					mode_custom = true;
					MAX_TILEX = customX;
					MAX_TILEY = customY;
					total_mines = customM;
					custom_wait_touch = true;
				}
				else
				{
					selector = newselector;
				}
			}
		}
	}
	else if (mode_game)
	{
		if ((kDown & KEY_UP) && (CASE_Y > 0)) CASE_Y--;
		else if ((kDown & KEY_DOWN) && (CASE_Y < MAX_TILEY-1)) CASE_Y++;
		else if ((kDown & KEY_RIGHT) && (CASE_X < MAX_TILEX-1)) CASE_X++;
		else if ((kDown & KEY_LEFT) && (CASE_X > 0)) CASE_X--;

		if ((kDown & KEY_TOUCH) && (inBox(Stylus, 320+(960-(MAX_TILEX*TILE_SIZEX))/2, (720-(MAX_TILEY*TILE_SIZEY))/2, 320+(960-(MAX_TILEX*TILE_SIZEX))/2 + MAX_TILEX*TILE_SIZEX, (720-(MAX_TILEY*TILE_SIZEY))/2 + MAX_TILEY*TILE_SIZEY)) && (!game_over))
		{
			CASE_X = (Stylus.px-320-(960-(MAX_TILEX*TILE_SIZEX))/2)/TILE_SIZEX;
			CASE_Y = (Stylus.py-(720-(MAX_TILEY*TILE_SIZEY))/2)/TILE_SIZEY;
			
			kDown |= KEY_A;
		}

		if ((kDown & KEY_A) && (!game_over))
		{
			game = true;
			Timer = true;

			if (level_courant_cache[CASE_X*MAX_TILEY + CASE_Y] != FLAG)
			{
				frame = 2;
			}
			if (kHeld & (KEY_L | KEY_R)) // modifier
			{
				timing = 15;
			}
		}
		if (kDown & KEY_X && level_courant_cache[CASE_X*MAX_TILEY + CASE_Y] == CACHER)
		{
			game = true;
			Timer = true;
			timing = 5;
		}

		if (Timer)
		{
			timing++;

			if ((timing >= 15) && (!game_over))
			{
				if (level_courant_cache[CASE_X*MAX_TILEY + CASE_Y] == 16)
				{
					frame = 2;
				}
				else
				{
					frame = 0;
				}
			}

			if (((kUp & KEY_TOUCH) || (kUp & KEY_A) || (kUp & KEY_X)) && timing >= 5)
			{
				if (timing < 15)//Plus long et c'est le flag
				{
					if ((cases_ouvertes == 0) && (level_courant_cache[CASE_X*MAX_TILEY + CASE_Y] == CACHER))
					{
						//On place les mines au hasard
						int nbr_mines = 0;

						while(nbr_mines != total_mines)
						{
							u8 colonnes = rand() % MAX_TILEY;
							u8 lignes = rand() % MAX_TILEX;
					
							if (level_courant[lignes*MAX_TILEY + colonnes] != BOMBE && (lignes != CASE_X || colonnes != CASE_Y))
							{
								level_courant[lignes*MAX_TILEY + colonnes] = BOMBE;
								nbr_mines++;
							}
						}

						//Pour chaque tile on compte le nombre de bombe voisine
						for(u8 lignes = 0; lignes < MAX_TILEX; lignes++)
						{
							for(u8 colonnes = 0; colonnes < MAX_TILEY; colonnes++)
							{
								update_level_courant(lignes, colonnes);
							}
						}

						frame = 0;
						rippleUncover(CASE_X, CASE_Y);
					}
					else if ((level_courant[CASE_X*MAX_TILEY + CASE_Y] == BOMBE) && (level_courant_cache[CASE_X*MAX_TILEY + CASE_Y] != FLAG)) //Mines sans flag
					{
						game_over = true;
						game = false;
						frame = 4;

						//La bombe rouge
						level_courant[CASE_X*MAX_TILEY + CASE_Y] = 12;//Mines rouge
						level_courant_cache[CASE_X*MAX_TILEY + CASE_Y] = 16;//Vide

						//On ouvre toutes les bombes non ouverte
						uncoverGameOver();
					}
					else if (level_courant_cache[CASE_X*MAX_TILEY + CASE_Y] == FLAG) //On enleve le flag
					{
						level_courant_cache[CASE_X*MAX_TILEY + CASE_Y] = CACHER;
						mines_restante++;
						frame = 0;
					}
					else if (level_courant_cache[CASE_X*MAX_TILEY + CASE_Y] == CACHER)
					{
						frame = 0;
						rippleUncover(CASE_X, CASE_Y);
					}
					else if (level_courant_cache[CASE_X*MAX_TILEY + CASE_Y] == 16)
					{
						frame = 0;
						uncoverNeighbours(CASE_X, CASE_Y);
					}
				}
				else
				{
					if (level_courant_cache[CASE_X*MAX_TILEY + CASE_Y] == CACHER)
					{
						//On met un flag
						level_courant_cache[CASE_X*MAX_TILEY + CASE_Y] = FLAG;
						if (mines_restante > 0) mines_restante--;
						frame = 0;
					}
					else if (level_courant_cache[CASE_X*MAX_TILEY + CASE_Y] == 16)
					{
						frame = 0;
						uncoverNeighbours(CASE_X, CASE_Y);
					}
				}

				Timer = false;
				timing = 0;
			}
		}

		//Conditions de victoire
		if (cases_ouvertes == (MAX_TILEY*MAX_TILEX)-total_mines)
		{
			frame = 3;
			game_over = true;
			game = false;

			for(u8 lignes = 0; lignes < MAX_TILEX; lignes++)
			{
				for(u8 colonnes = 0; colonnes < MAX_TILEY; colonnes++)
				{
					if ((level_courant[lignes*MAX_TILEY + colonnes] == BOMBE))
					{
						level_courant_cache[lignes*MAX_TILEY + colonnes] = FLAG;
					}
				}
			}

			mines_restante = 0;

			if (selector <= 2)
			{
				if (chrono-1 < highscore[selector])
				{
					highscore[selector] = chrono-1;
					data_changed = true;
				}
			}
		}

		if ((kHeld & (KEY_MINUS | KEY_Y)) || (game_over && kDown & (KEY_A | KEY_X | KEY_TOUCH)) || (game_over && anim && kHeld & (KEY_A | KEY_X | KEY_TOUCH)))
		{
			frame = 1;
			anim = true;
		}

		if (((kUp & (KEY_MINUS | KEY_Y)) || (game_over && kUp & (KEY_A | KEY_X | KEY_TOUCH))) && anim && !(kHeld & (KEY_MINUS | KEY_Y) || (game_over && kDown & (KEY_A | KEY_X | KEY_TOUCH)) || (game_over && anim && kHeld & (KEY_A | KEY_X | KEY_TOUCH))))
		{
			anim = false;
			Restart_Game();
		}

		if (kDown & (KEY_B))
		{
			mode_title = true;
			mode_game = false;
			game = false;
		}
	}
	else if (mode_custom)
	{
		if (kDown & (KEY_B))
		{
			mode_title = true;
			mode_custom = false;
		}
		else if ((kDown & KEY_UP) && (MAX_TILEY < 14)) MAX_TILEY++;
		else if ((kDown & KEY_DOWN) && (MAX_TILEY > 2)) MAX_TILEY--;
		else if ((kDown & KEY_RIGHT) && (MAX_TILEX < 19)) MAX_TILEX++;
		else if ((kDown & KEY_LEFT) && (MAX_TILEX > 2)) MAX_TILEX--;
		else if ((kDown & KEY_R) && (total_mines < MAX_TILEX*MAX_TILEY-1)) total_mines++;
		else if ((kDown & KEY_L) && (total_mines > 1)) total_mines--;
		else if (custom_wait_touch)
		{
			if (!(kHeld & KEY_TOUCH))
			{
				custom_wait_touch = false;
			}
		}
		else if ((kHeld & KEY_TOUCH) && inBox(Stylus, 460, 189, 823, 219))
		{
			MAX_TILEY = (Stylus.px - 460)/30 + 2;
		}
		else if ((kHeld & KEY_TOUCH) && inBox(Stylus, 385, 369, 898, 399))
		{
			MAX_TILEX = (Stylus.px - 385)/30 + 2;
		}
		else if ((kHeld & KEY_TOUCH) && inBox(Stylus, 244, 549, 1042, 579))
		{
			total_mines = (Stylus.px - 244)/3;
		}

		else if (((kDown & KEY_TOUCH) && inBox(Stylus, 1146, 638, 1280, 720)) || kDown & KEY_A)
		{
			mode_game = true;
			mode_custom = false;

			Restart_Game();
		}
		if (!mode_custom && (MAX_TILEX != customX || MAX_TILEY != customY || total_mines != customM))
		{
			customX = MAX_TILEX;
			customY = MAX_TILEY;
			customM = total_mines;
			data_changed = true;
		}
	}
}

int main()
{
	SDL_Init(SDL_INIT_EVERYTHING);
	IMG_Init(IMG_INIT_PNG);
	romfsInit();

	srand(time(0));

	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 1024);
	Mix_AllocateChannels(2);

	highscore[0] = 999;
	highscore[1] = 999;
	highscore[2] = 999;

	customX = 2;
	customY = 2;
	customM = 1;
	selector = 0;

	savepath = SAVEPATH;
	struct stat s;
	int err = stat(SAVEPATH, &s);

	if(err == -1)
	{
		savepath = "./";
	}
	else if(!S_ISDIR(s.st_mode))
	{
		savepath = "./";
	}

	sprintf(filename, "%s%s", savepath, SAVEFILE);
	FILE* save = fopen(filename, "rb");

	if (save)
	{
		fread(highscore, sizeof(u16), 3, save);
		fread(&customX, sizeof(u8), 1, save);
		fread(&customY, sizeof(u8), 1, save);
		fread(&customM, sizeof(int), 1, save);
		fread(&selector, sizeof(u8), 1, save);
		fclose(save);
	}

	saved_selector = selector;
	data_changed = false;

	// Create an SDL window & renderer
	window = SDL_CreateWindow("Main-Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	//BG
	surface = IMG_Load("romfs:/resources/BG_BOTTOM_MENU.png");
	background[0].texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);

	surface = IMG_Load("romfs:/resources/BG_BOTTOM_GAME.png");
	background[2].texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);

	surface = IMG_Load("romfs:/resources/BG_BOTTOM_CUSTOM.png");
	background[4].texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);

	//Sprite
	surface = IMG_Load("romfs:/resources/SELECTOR.png");
	sprite[0].texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);
	
	surface = IMG_Load("romfs:/resources/CURSOR.png");
	sprite[1].texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);

	//Emoticon
	surface = IMG_Load("romfs:/resources/EMOTICONS.png");
	smiley.texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);

	//Nombre
	surface = IMG_Load("romfs:/resources/NUMBERS.png");
	chiffre.texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);

	//Number
	surface = IMG_Load("romfs:/resources/NUMBER.png");
	number.texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);

	//Tiles
	surface = IMG_Load("romfs:/resources/TILES.png");
	tile.texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);

	mode_custom = false;
	mode_game = false;
	mode_title = true;
	custom_wait_touch = true;

	MAX_TILEX = 2;
	MAX_TILEY = 2;
	total_mines = 1;

	Mix_Music *musique;
	musique = Mix_LoadMUS("romfs:/resources/sounds/World-of-Automatons_Looping.mp3");
	Mix_PlayMusic(musique, -1);

	son1 = Mix_LoadWAV("romfs:/resources/sounds/UI_Quirky19.wav");

	while (appletMainLoop())
	{
		hidScanInput();

		kDown = hidKeysDown(CONTROLLER_P1_AUTO);
		kHeld = hidKeysHeld(CONTROLLER_P1_AUTO);
		kUp = hidKeysUp(CONTROLLER_P1_AUTO);

		hidTouchRead(&Stylus, 0);

		if (kDown & KEY_PLUS) break;

		if (!game_over && game)
		{
			tempsActuel = SDL_GetTicks();

			if (tempsActuel - tempsPrecedent > 1000)
			{
				chrono++;
				if (chrono > 1) Mix_PlayChannel(1, son1, 0);
				if (chrono >= 1000) chrono = 1000;
				tempsPrecedent = tempsActuel;
			}
		}

		manageInput();

		printGame();
	}

	if (mode_custom && (MAX_TILEX != customX || MAX_TILEY != customY || total_mines != customM))
	{
		customX = MAX_TILEX;
		customY = MAX_TILEY;
		customM = total_mines;
		data_changed = true;
	}
	if (selector != saved_selector)
	{
		data_changed = true;
	}
	if (data_changed)
	{
		FILE* save = fopen(filename, "wb");
		if (save)
		{
			fwrite(highscore, sizeof(u16), 3, save);
			fwrite(&customX,  sizeof(u8),  1, save);
			fwrite(&customY,  sizeof(u8),  1, save);
			fwrite(&customM,  sizeof(int), 1, save);
			fwrite(&selector, sizeof(u8),  1, save);
			fclose(save);
		}
	}

	Mix_FreeChunk(son1);
	Mix_CloseAudio();

	SDL_Quit();				// SDL cleanup
	return EXIT_SUCCESS; 	// Clean exit to HBMenu
}
