//	To do:
//	fix crashes
//		div by zero?
//		cases when looking in a cardinal direction?
//	add Front and Back walls lel
//	scan in maps from a file
//		make it so you don't enter them backwards lmao
//		maybe a way to scan in a png or something?
//			maybe a raw would be easier
//	collision?
//	fix funky column height curve
//	prevent OOB on map array
//	settable map sizes?

#include <stdio.h>
#include <math.h>
#include <cstdlib>
#include <conio.h>

#define PI 3.14159265

struct engineSettings
{
	int h;
	int w;
	
	float fov; // Radians
	
	float colDistMax;   // Distance at which a column will fill the whole screen
	float colDistRend;  // Walls cut off at this distance
	float colDistCurve; // Affects strength of curve concavity. Range: (0,1)
};

struct mapFile
{
	bool walls[10][10];
	
	// Used map area
	int sizeY;
	int sizeX;
	
	// Player starting conditions
	float startX;
	float startY;
	float startTheta; // Radians
};

struct player
{
	float x;
	float y;
	float theta; // Radians
	
	float speedMov;
	float speedTurn;
};

struct texturePack
{
	char WallLR;
	char WallFB;
	char Ceiling;
	char Floor;
	char Tran;
	char TranNeg;
	char TranPos;
	char TranCcu;
	char TranCcd;
	char TranWall;
};

// Takes a theta value and gives you the direction of north
char northArrow(float theta)
{
	// Convert to degrees
	theta *= 180/PI;
	
	// Find octant
	int octant = 0;
	for (; theta-22.5 > 0; octant++) { theta -= 45; }
	
	// Convert octant to char
	char arrow;
	switch(octant)
	{	                                 // North is:
		case 0:  { arrow =  27; break; } // L
		case 1:  { arrow = 201; break; } // UL
		case 2:  { arrow =  24; break; } // U
		case 3:  { arrow = 187; break; } // UR
		case 4:  { arrow =  26; break; } // R
		case 5:  { arrow = 188; break; } // DR
		case 6:  { arrow =  25; break; } // D
		case 7:  { arrow = 200; break; } // DL
		case 8:  { arrow =  27; break; } // L
		default: { arrow = '?'; break; } // ?
	}
	
	return arrow;
}

// Returns the sign of the value (-1, 0, or 1)
int sgn(float num)
{
	if (num > 0) { return  1; }
	if (num < 0) { return -1; }
	return  0;
}

// Wraps theta values to [0, 2*PI)
float wrap(float &theta)
{
	if (theta >= 2*PI)  { theta -= 2*PI; }
	else if (theta < 0) { theta += 2*PI; }
	
	return theta;
}

int main()
{
	// Initialize engine settings
	struct engineSettings eng;
	eng.h = 23*2;
	eng.w = 79*2;
	eng.fov = 90 * (PI/180);
	eng.colDistMax = 0.3;
	eng.colDistRend = 15;
	eng.colDistCurve = 0.7;
	
	// Define map and start to read it in
	struct mapFile map;
	FILE* pMap = fopen("default.map", "r");
	fscanf(pMap, "%*[^\n]\n"); // Skip header
	
	// Read map meta data
	float thetaDegrees = 0; // We will convert theta to radians
	fscanf(
		pMap, "%d,%d,%f,%f,%f%*[^\n]\n"
		, &map.sizeX, &map.sizeY
		, &map.startX, &map.startY
		, &thetaDegrees
	);
	map.startTheta = thetaDegrees * (PI/180);
		
	// Read map walls
	fscanf(pMap, "%*[^\n]\n"); // Skip blank line
	int temp = 0; // Reading directly into the bool gave wierd errors
	for (int y = map.sizeY-1; y >= 0; y--)
	{
		for (int x = 0; x < map.sizeX; x++)
		{
			fscanf(pMap, "%d,", &temp);
			if      (temp == 1) { map.walls[y][x] = true;  }
			else if (temp == 0) { map.walls[y][x] = false; }
			else
			{
				printf("Error: invalid map file");
				exit(0);
			}
		}
	}
	fclose(pMap);
	
	// Place player in map and set attributes
	struct player plr;
	plr.x = map.startX;
	plr.y = map.startY;
	plr.theta = map.startTheta;
	plr.speedMov = 0.1;
	plr.speedTurn = 10 * (PI/180);
	
	// Set texture pack
	struct texturePack tex;
	tex.WallLR  = '#';
	tex.WallFB  = '7';
	tex.Ceiling = ' ';
	tex.Floor   = '.';
	tex.Tran    = '_';
	tex.TranNeg = '\\';
	tex.TranPos = '/';
	tex.TranCcu = 'V';
	tex.TranCcd = '^';
	tex.TranWall= 'L';
		
	// Initialize screen buffer
	char frameBuf[eng.h + 1][eng.w]; // Using last row to store wall tex
	for (int x = 0; x < eng.w; x++)
	{
		for (int y = 0; y < eng.h + 1; y++)
		{
			frameBuf[y][x] = ' ';
		}
	}
	
	// Declare pointer to log file (only used when dumping)
	FILE* pLog;
	
	// Loop per frame
	char input = ' ';
	while (input != 'h')
	{
		// Wrap player theta
		wrap(plr.theta);
		
		// If dumping, initiate log file
		if (input == 'd')
		{
			// Create/erase log file (will close at UI dump scene)
			pLog = fopen("log.csv", "w");
			
			// Engine info
			fprintf(pLog, "eng.h,eng.w,eng.fov,\n");
			fprintf(
				pLog, "%d,%d,%f,\n,\n"
				, eng.h, eng.w, eng.fov*(180/PI)
			);
			
			// Player info
			fprintf(pLog, "plr.x,plr.y,plr.theta,\n");
			fprintf(
				pLog, "%f,%f,%f,\n,\n"
				, plr.x, plr.y, plr.theta*(180/PI)
			);
			
			// Ray table header
			fprintf(pLog, "Ray Table,\n");
			fprintf(pLog, "#,theta,dist,colH,\n");
		}
		
		// Loop for each ray
		float raySpacing = eng.fov / eng.w;
		float rayStartTheta = plr.theta + (eng.fov / 2);
		for (int r = 0; r < eng.w; r++)
		{
			// Find angle of ray
			float rayTheta = rayStartTheta - (raySpacing / 2) - (raySpacing * r);
			
			// Wrap rayTheta
			wrap(rayTheta);
						
			// Find ray length from player to wall
			char rayTex = ' ';
			float rayDist = 0;
			int wallX = plr.x;
			int wallY = plr.y;
			float cellX = plr.x - (int)plr.x;
			float cellY = plr.y - (int)plr.y;
			if (rayTheta == PI/2.0 || rayTheta == 3.0*(PI/2.0) ) { rayTheta += .01; } // Temp fix
			for (int n = 0; !map.walls[wallY][wallX]; n++)
			{
				// Set ray texture
				rayTex = tex.WallLR;
				
				// Find dist to wall N
				// L and R wall equations are merged
				rayDist = (
					(
						(n + 0.5) * sgn(cos(rayTheta)) //SIGN not sin! -1 or 1
						+ 0.5 - cellX
					)
					/ cos(rayTheta)
				);
				
				// Break if reached render distance
				if (rayDist > eng.colDistRend) { break; }
				
				// Find map coords to check for wall
				wallX = (int)plr.x + (n+1) * sgn(cos(rayTheta)); // L and R equations merged
				wallY = (int)(plr.y + rayDist * sin(rayTheta));  // L and R were the same
			}
			
			// Column height
			float colH = 0;
			if (rayDist <= eng.colDistRend) // Leave at 0 if past render distance
			{
				// Adjust ray for aberration?
				rayDist *= cos(plr.theta - rayTheta);
				
				// Calculate column height
				colH = pow(eng.colDistCurve, rayDist - eng.colDistMax) * eng.h;
			}
			
			// Store to frame buffer
			frameBuf[eng.h][r] = rayTex; // Note wall texture for edge function
			for (int y = 0; y < eng.h; y++)
			{
				// Wall
				if (
					   y >  (eng.h - colH) / 2
					&& y <= (eng.h - colH) / 2 + colH
				)
				{
					frameBuf[y][r] = rayTex;
				}
				// Floor
				else if ( y >  (eng.h - colH) / 2 + colH )
				{
					frameBuf[y][r] = tex.Floor;
				}
				// Ceiling
				else if ( y <= (eng.h - colH) / 2 )
				{
					frameBuf[y][r] = tex.Ceiling;
				}
				// Unknown case
				else
				{
					frameBuf[y][r] = '?';
				}
			}
			
			// Dump rays to log, if desired
			if (input == 'd')
			{
				fprintf(
					pLog, "%d,%f,%f,%f,\n"
					, r, rayTheta*(180/PI), rayDist, colH
				);
			}
		}
		
		// Draw edges
		for (int x = 0; x < eng.w; x++)
		{
			// Wall tex for this column
			char colTex = frameBuf[eng.h][x];
			
			// Wall tex left column
			char colTexL = colTex;
			if (x >= 1) // OOB prevention
			{ colTexL = frameBuf[eng.h][x-1]; }
			
			// Wall tex right column
			char colTexR = colTex;
			if (x <= eng.w - 2) // OOB prevention
			{ colTexR = frameBuf[eng.h][x+1]; }
			
			for (int y = 0; y < eng.h; y++)
			{
				// Ceiling trans
				if (
					   frameBuf[y  ][x  ] == tex.Ceiling
					&& y + 1 < eng.h // OOB prevention
					&& frameBuf[y+1][x  ] == colTex
				)
				{
					// OOB prevention
					if (
						   x == 0
						|| x == eng.w - 1
					)
					{
						frameBuf[y][x] = tex.Tran;
					}
					// Concave up trans
					else if (
						   frameBuf[y  ][x-1] == colTexL
						&& frameBuf[y  ][x+1] == colTexR
					)
					{
						frameBuf[y][x] = tex.TranCcu;
					}
					// Neg trans
					else if (frameBuf[y  ][x-1] == colTexL)
					{
						frameBuf[y][x] = tex.TranNeg;
					}
					// Pos trans
					else if (frameBuf[y  ][x+1] == colTexR)
					{
						frameBuf[y][x] = tex.TranPos;
					}
					// Normal case
					else
					{
						frameBuf[y][x] = tex.Tran;
					}
				}
				// Floor trans
				else if (
					   frameBuf[y  ][x  ] == tex.Floor
					&& y - 1 >= 0 // OOB prevention
					&& frameBuf[y-1][x  ] == colTex
				)
				{
					// OOB prevention
					if (
						   x == 0
						|| x == eng.w - 1
					)
					{
						frameBuf[y][x] = tex.Tran;
					}
					// Concave up
					else if (
						frameBuf[y-1][x+1] != colTexR
						&& (
							   frameBuf[y-1][x-1] == tex.Tran
							|| frameBuf[y-1][x-1] == tex.TranNeg
						)
					)
					{
						frameBuf[y][x] = tex.TranCcu;
					}
					// Pos trans
					else if (frameBuf[y-1][x+1] != colTexR)
					{
						frameBuf[y][x] = tex.TranPos;
					}
					// Neg trans
					else if (frameBuf[y-1][x-1] != colTexL)
					{
						frameBuf[y][x] = tex.TranNeg;
					}
					// Normal case
					else
					{
						// Baseboard wall transition
						if (colTexL != colTex)
						{ frameBuf[y][x] = tex.TranWall; }
						else
						{ frameBuf[y][x] = tex.Tran; }
					}
				}
			}
		}
		
		// Pile frame buffer into a single string
		//                 ((Line + \n) * #lines) + \0
		int printBufSize = ((eng.w + 1) * eng.h) + 1;
		char printBuf[printBufSize];
		for (int y = 0; y < eng.h; y++)
		{
			int lineOffset = y * (eng.w + 1);
			
			for (int x = 0; x < eng.w; x++)
			{
				printBuf[x + lineOffset] = frameBuf[y][x];
			}
			
			printBuf[eng.w + lineOffset] = '\n';
		}
		
		// Print frame. Only uses one printf. Much faster!
		system("cls");
		printf("%s", printBuf);
		
		// UI scenes
		// Options menu
		if (input == 'm')
		{
			// Status Bar
			printf("b = back | d = dump\n> ");
			
			// Get input
			input = getch();
			switch(input)
			{
				case 'b': { break; } // Return to main screen
				case 'd': {	break; } // Enter dump screen
				default:
				{
					// Re-enter options menu
					input = 'm';
					break;
				}
			}
			
			// Clear input unless it's to change scenes
			{
				if (
					   input != 'd'
					&& input != 'm'
				)
				{ input = ' '; }
			}
		}
		// Dump
		else if (input == 'd')
		{
			// Status Bar
			printf("Current scene rays dumped to `log.csv`\n> ");
			
			// Close dump pointer
			fclose(pLog);
			
			// Get input
			input = ' '; // Return to main scene
			system("pause");
		}
		// Controls
		else if (input == 'c')
		{
			// Status Bar
			printf("h = halt (quit) | wasd = movement | q/e = look\n> ");
			
			// Get input
			input = ' '; // return to main scene
			system("pause");
		}
		// Main scene
		else
		{
			// Status bar
			printf
			(
				"pos:(%5.2f,%5.2f) | theta:%3.0f%c | N:%c | m = menu | c = controls\n> "
				, plr.x, plr.y
				, plr.theta * (180/PI), 248 // Degree symbol
				, northArrow(plr.theta)
			);
			
			// Get input
			input = getch();
			switch(input)
			{
				case 'w':
				{
					plr.x += plr.speedMov * cos(plr.theta);
					plr.y += plr.speedMov * sin(plr.theta);
					break;
				}
				case 'a':
				{
					plr.x -= plr.speedMov * sin(plr.theta);
					plr.y += plr.speedMov * cos(plr.theta);
					break;
				}
				case 's':
				{
					plr.x -= plr.speedMov * cos(plr.theta);
					plr.y -= plr.speedMov * sin(plr.theta);
					break;
				}
				case 'd':
				{
					plr.x += plr.speedMov * sin(plr.theta);
					plr.y -= plr.speedMov * cos(plr.theta);
					break;
				}
				case 'q': { plr.theta += plr.speedTurn; break; }
				case 'e': { plr.theta -= plr.speedTurn; break; }
				default:  { break; }
				case 'h': { break; } // Halt
				case 'm': { break; } // Enter menu
				case 'c': { break; } // Show controls
			}
			
			// Clear input unless it's to change scenes
			{
				if (
					   input != 'h'
					&& input != 'm'
					&& input != 'c'
				)
				{ input = ' '; }
			}
		}
	}
	
	return 0;
}
