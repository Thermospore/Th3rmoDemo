/*	To do:
bug fixes
	fix crashes
		div by zero?
		cases when looking in a cardinal direction?
	prevent OOB on map array
	shit gets broken at render distance
		only seems to be an issue with odd eng.h vals
	
general improvements
	find better names for player thetas for hor and vert
	double check default colH?
	move wallH to map struct
	prevent edge drawing from bleeding into different walls
	make the walls array dynamic?
		so it isn't hardcoded to a certain size
	implement render distance into LR/FB raycasting thing
		to improve efficiency
	move loading maps to a function
		this way you can add loading maps as a menu option
	fix funky column height curve at different vtheta, wallh, and plrh values
		maybe vert col rays need some sort of aberation correction as well?
	2nd frame buff array to store col info
		raytex, wall position, etc
	add a constant for rad to deg and deg to rad
	move commas between variables a line up in multiline printfs?
		
new features
	collision?
	minimap!
	supersampling would be cool!
	full raycasting on vertical rays
	be able to have walls that don't touch the floor
		and be able to see walls underneath behind them
	revamp UI engine
	cast rays so they are spaced out evenly by wall dist, not by theta
	settings file?
*/

#include <stdio.h>
#include <math.h>
#include <cstdlib>
#include <conio.h>

#define PI 3.14159265
#define SQR_RATIO 1.908213 // Ratio to get visibly square walls on default console settings

struct engineSettings
{
	int w;
	int h;
	
	float fov; // Radians
	
	float rendDist;  // Walls cut off at this distance
	
	float wallH; // Should move to map...
};

struct mapFile
{
	bool walls[50][50];
	
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
	float h;
	
	float theta; // Radians
	float vTheta;
	
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
	eng.w = 80 - 1; // Subtract 1 column so you don't get two newlines
	eng.h = 25 - 2; // Subtract 2 lines for UI
	eng.fov = 90 * (PI/180);
	eng.rendDist = 50;
	eng.wallH = SQR_RATIO;
	
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
	plr.h = SQR_RATIO / 2;
	plr.theta = map.startTheta;
	plr.vTheta = 90*(PI/180);
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
	
	// Declare pointer to log file (only used when dumping)
	FILE* pLog;
	
	// Loop per frame
	char input = ' ';
	while (input != 'h')
	{	
		// Initialize screen buffer
		char frameBuf[eng.h + 1][eng.w]; // Using last row to store wall tex
		for (int x = 0; x < eng.w; x++)
		{
			for (int y = 0; y < eng.h + 1; y++)
			{
				frameBuf[y][x] = ' ';
			}
		}
	
		// Wrap player theta
		wrap(plr.theta);
		
		// If dumping, initiate log file
		if (input == 'd')
		{
			// Create/erase log file (will close at UI dump scene)
			pLog = fopen("log.csv", "w");
			
			// Engine info
			fprintf(pLog, "eng.h,eng.w,eng.fov (°),eng.rendDist,eng.wallH\n");
			fprintf(
				pLog, "%d,%d,%f,%f,%f\n,\n"
				, eng.h, eng.w, eng.fov*(180/PI), eng.rendDist, eng.wallH
			);
			
			// Player info
			fprintf(pLog, "plr.x,plr.y,plr.h,plr.theta (°),plr.vTheta (°)\n");
			fprintf(
				pLog, "%f,%f,%f,%f,%f\n,\n"
				, plr.x, plr.y, plr.h, plr.theta*(180/PI), plr.vTheta*(180/PI)
			);
			
			// Ray table header
			fprintf(pLog, "Ray Table,\n");
			fprintf(pLog, "r,rayTheta (°),rayDist,wallX,wallY,thetaWT (°),thetaWB (°),colT,colB,rayTex\n");
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
			
			// Temp fix for cardinal directions
			if (
				rayTheta == 0
				|| rayTheta == PI/2
				|| rayTheta == 2*PI
				|| rayTheta == 3*(PI/2)
			) { rayTheta += .01; }
						
			// Find ray length from player to wall
			// We check Front/Back and Left/Right walls separately
			char rayTex = ' ';
			
			float cellX = plr.x - (int)plr.x; // Position in current cell
			float cellY = plr.y - (int)plr.y;
			
			float rayDist = 0;
			float rayDistFB = (
					( 0.5 * (sgn(sin(rayTheta)) + 1) - cellY )
					/ sin(rayTheta)
				);
			float rayDistLR = (
					( 0.5 * (sgn(cos(rayTheta)) + 1) - cellX )
					/ cos(rayTheta)
				);
			
			int wallNFB = 0; // Number of walls away from current cell
			int wallNLR = 0;
			
			int wallX = plr.x; // Coords of wall we are checking
			int wallY = plr.y;
			
			while (!map.walls[wallY][wallX])
			{
				// Check closest wall
				// Front and Back walls
				if (rayDistFB < rayDistLR)
				{
					rayDist = rayDistFB;
					rayTex = tex.WallFB;
					
					wallY = (int)plr.y + (wallNFB+1) * sgn(sin(rayTheta)); // F and B equations merged
					wallX = (int)(plr.x + rayDist * cos(rayTheta)); // F and B were the same
					
					// Update to next wall for the next loop
					wallNFB++;
					rayDistFB = (
						(
							(wallNFB + 0.5) * sgn(sin(rayTheta)) //SIGN not sin! -1 or 1
							+ 0.5 - cellY
						)
						/ sin(rayTheta)
					);
				}
				// Left and Right walls
				else
				{
					rayDist = rayDistLR;
					rayTex = tex.WallLR;
					
					wallX = (int)plr.x + (wallNLR+1) * sgn(cos(rayTheta)); // L and R equations merged
					wallY = (int)(plr.y + rayDist * sin(rayTheta)); // L and R were the same
					
					// Update to next wall for the next loop
					wallNLR++;
					rayDistLR = (
						(
							(wallNLR + 0.5) * sgn(cos(rayTheta)) //SIGN not sin! -1 or 1
							+ 0.5 - cellX
						)
						/ cos(rayTheta)
					);
				}
			}
			
			// Column height
			float thetaWT = -1; // Init at -1 so we can detect if it's not touched
			float thetaWB = -1;
			float colT = eng.h / 2.0; // This might be wrong
			float colB = eng.h / 2.0;
			if (rayDist <= eng.rendDist) // Leave at 0 if past render distance
			{
				// Adjust ray for aberration?
				rayDist *= cos(plr.theta - rayTheta);
			
				// Find angles to Top and Bottom of wall
				thetaWT = PI/2 + atan((eng.wallH-plr.h) / rayDist);
				thetaWB = atan(rayDist / plr.h);
				
				// Find where the column should be placed on the screen	
				colT = (plr.vTheta + eng.fov/2 - thetaWT)*(eng.h/eng.fov);
				colB = (plr.vTheta + eng.fov/2 - thetaWB)*(eng.h/eng.fov);
				
				// Leaving this here as a tribute, since it never got to see the light of day :(
				// colH = eng.h * ((2/PI)*atan(0.5 / rayDist) - (2/PI)*atan(2*rayDist) + 1);
			}
			
			// Store to frame buffer
			frameBuf[eng.h][r] = rayTex; // Note wall texture for edge function
			for (int y = 0; y < eng.h; y++)
			{
				// Wall
				if (
					   y >= colT - 0.5
					&& y <= colB - 0.5
				)
				{
					frameBuf[y][r] = rayTex;
				}
				// Floor
				else if (y > colB - 0.5)
				{
					frameBuf[y][r] = tex.Floor;
				}
				// Ceiling
				else if (y < colT - 0.5)
				{
					frameBuf[y][r] = tex.Ceiling;
				}
				// Unknown case
				else
				{
					frameBuf[y][r] = '?';
				}
			}
			
			// Possibly dump rays to log
			if (input == 'd')
			{
				fprintf(
					pLog, "%d,%f,%f,%d,%d,%f,%f,%f,%f,%c\n"
					, r, rayTheta*(180/PI), rayDist, wallX, wallY
					, thetaWT*(180/PI), thetaWB*(180/PI), colT, colB, rayTex
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
		printBuf[printBufSize-1] = '\0'; // End of string character
		
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
			printf("b = back | d = dump | r = resolution\n> ");
			
			// Get input
			input = getch();
			switch(input)
			{
				case 'b': { break; } // Return to main screen
				case 'd': {	break; } // Enter dump screen
				case 'r': { break; } // Enter res select screen
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
					&& input != 'r'
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
			system("pause");
			
			// Return to main scene
			input = ' ';
		}
		// Resolution change
		else if (input == 'r')
		{
			// Status Bar
			printf("Enter console res (\"[w] [h]\")\n>" );
			
			// Get input
			int consoleW;
			int consoleH;
			scanf("%d %d", &consoleW, &consoleH);
			eng.w = consoleW - 1; // Subtract 1 column so you don't get two newlines
			eng.h = consoleH - 2; // Subtract 2 lines for UI
			
			// Return to main scene
			input = ' ';
		}
		// Controls
		else if (input == 'c')
		{
			// Status Bar
			printf("h = halt (quit) | wasd = movement | q/e = look\n> ");
			
			// Get input
			system("pause");
			
			// return to main scene
			input = ' ';
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
