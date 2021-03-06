/*	To-do:
bug fixes
	fix crashes
		div by zero?
		cases when looking in a cardinal direction?
	prevent OOB on map array
	
general improvements
	map stuff
		map selection
			show current map name in selection menu
			maybe list available maps?
			prevent entering non existant or invalid maps
			not requre entering ".map"
		make the walls array dynamic?
			so it isn't hardcoded to a certain size
		add starting movement speed to struct
	work out better names for ray shit
		maybe a ray struct
	prevent edge drawing from bleeding into different walls
		maybe 2nd frame buff array to store col info
			raytex, wall position, etc
	implement render distance into LR/FB raycasting thing
		to improve efficiency
		
new features
	collision?
		prevent entering or tracing rays into negative map values
			maybe allow but just cut off entering map array?
			when in negative map, use sign of tangent
				to tell if you will be able to cast rays to positive map values
	minimap!
		use box drawing characters
		scaling?
		rotation?
	supersampling would be cool!
	wall stuff
		be able to have walls that don't touch the floor
			and be able to see walls underneath behind them
		be able to see top and underside of walls?
	new UI engine
		maybe as a stop gap, add char var for menu subchoice
	settings file?
	be able to look up and down
		wrapping?
			flip player around when phi is greater than 180deg?
			or have it so your head is bent backwards and you can see walls upsideown lol
*/

#include <stdio.h>
#include <math.h>
#include <cstdlib>
#include <conio.h>

#define PI 3.14159265
#define RTD 180/PI // Radians to degrees
#define DTR PI/180 // Degrees to radians

struct engineSettings
{
	// Resolution
	int renW;       // 3D render resolution, not including UI
	int renH;
	int fontH;      // Console font resolution
	int fontW;
	
	// Visibility
	float fov;      // Base horizontal FOV in rad
	float fovPhi;   // Vertical FOV derived from hor FOV, eng h&w, and character h&w
	float rendDist; // Walls cut off at this distance
};

struct mapFile
{
	// Map
	bool walls[150][150]; // 1 for box, 0 for no box
	float wallH; // Height of walls
	int sizeY;   // Size of map
	int sizeX;
	
	// Player starting conditions
	float startX;
	float startY;
	float startH;
	float startTheta; // Radians
	float startPhi;
};

struct player
{
	// Position in map
	float x;
	float y;
	float h; // Height
	
	// Angles
	float theta; // Hor ang in rad. 0deg = right, 90deg = forward
	float phi;   // vert ang in rad. 0deg = straight down, 90deg = straight forward
	
	// Rates
	float speedMov;
	float speedTurn;
};

struct texturePack
{
	char wallLR;   // Left and Right walls
	char wallFB;   // Front and Back walls
	char ceiling;
	char floor;
	char tran;     // When transitioning between texture types
	char tranNeg;  // Negative slope
	char tranPos;  // Pos slope
	char tranCcu;  // Concave up
	char tranCcd;  // Concave down
	char tranWall; // Used on floor baseboard for when there is a different wall type adjacent
	char unknown;  // Used when you don't know what the hell is going on
};

// Takes a theta value and gives you the direction of north
char northArrow(float theta)
{
	// Convert to degrees
	theta *= RTD;
	
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

// Reads in a map file, places the player in it, and returns the map file
struct mapFile readMap(const char* mapName, struct player* plr)
{
	// Define mapFile struct and start to read it in
	struct mapFile map;
	FILE* pMap = fopen(mapName, "r");
	fscanf(pMap, "%*[^\n]\n"); // Skip header
	
	// Read map meta data
	float thetaDegrees = 0; // We will convert theta to radians
	float phiDegrees   = 0;
	fscanf(
		pMap, "%d,%d,%f,%f,%f,%f,%f,%f%*[^\n]\n"
		, &map.sizeX, &map.sizeY, &map.wallH
		, &map.startX, &map.startY,  &map.startH
		, &thetaDegrees, &phiDegrees
	);
	map.startTheta = thetaDegrees * DTR;
	map.startPhi   = phiDegrees   * DTR;
		
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
	
	// Place player in map
	plr->x = map.startX;
	plr->y = map.startY;
	plr->h = map.startH;
	plr->theta = map.startTheta;
	plr->phi   = map.startPhi;

	return map;
}

int main()
{
	// Initialize engine settings
	struct engineSettings eng;
	eng.fontH = 18;
	eng.fontW = 10;
	eng.renW = 80 - 1; // Subtract 1 column so you don't get two newlines
	eng.renH = 25 - 2; // Subtract 2 lines for UI
	eng.fov = 90 * DTR;
	eng.fovPhi; // We will recalculate this each frame, in case engine settings are changed
	eng.rendDist = 50;
	
	// Set player atributes
	struct player plr;
	plr.speedMov = 0.1;
	plr.speedTurn = 10 * DTR;
	
	// Read default map and place the player in it
	struct mapFile map = readMap("default.map", &plr);
		
	// Set texture pack
	struct texturePack tex;
	tex.wallLR  = '#';
	tex.wallFB  = '7';
	tex.ceiling = ' ';
	tex.floor   = '.';
	tex.tran    = '_';
	tex.tranNeg = '\\';
	tex.tranPos = '/';
	tex.tranCcu = 'V';
	tex.tranCcd = '^';
	tex.tranWall= 'L';
	
	// Declare pointer to log file (only used when dumping)
	FILE* pLog;
	
	// Loop per frame
	char input = ' ';
	while (input != 'h')
	{	
		// Initialize screen buffer
		// Y is 0 at top row! Sorta upside-down, I know
		char frameBuf[eng.renH + 1][eng.renW]; // Using last row to store wall tex
		for (int x = 0; x < eng.renW; x++)
		{
			for (int y = 0; y < eng.renH + 1; y++)
			{
				frameBuf[y][x] = ' ';
			}
		}
		
		// Update vertical FoV
		eng.fovPhi = (
			2
			* atan(
				  (eng.renH * eng.fontH * tan(eng.fov/2))
				/ (eng.renW * eng.fontW)
			)
		);
	
		// Wrap player theta
		wrap(plr.theta);
		
		// If dumping, initiate log file
		if (input == 'd')
		{
			// Create/erase log file (will close at UI dump scene)
			pLog = fopen("log.csv", "w");
			
			// Engine info
			fprintf(pLog, "eng.renH,eng.renW,eng.fontH,eng.fontW,eng.fov (�),eng.fovPhi (�),eng.rendDist,eng.wallH\n");
			fprintf(
				pLog, "%d,%d,%d,%d,%f,%f,%f,%f\n,\n"
				, eng.renH, eng.renW, eng.fontH, eng.fontW
				, eng.fov * RTD, eng.fovPhi * RTD, eng.rendDist, map.wallH
			);
			
			// Player info
			fprintf(pLog, "plr.x,plr.y,plr.h,plr.theta (�),plr.phi (�)\n");
			fprintf(
				pLog, "%f,%f,%f,%f,%f\n,\n"
				, plr.x, plr.y, plr.h, plr.theta * RTD, plr.phi * RTD
			);
			
			// Ray table header
			fprintf(pLog, "Ray Table,\n");
			fprintf(pLog, "r,rayTheta (�),rayDist,wallX,wallY,colT,colB,rayTex\n");
		}
		
		// Loop for each theta ray
		for (int r = 0; r < eng.renW; r++)
		{
			// Find angle of ray
			float rayTheta = (
				atan(
					  ( (eng.renW / 2.0) - 0.5 - r )
					/ ( eng.renW / (2 * tan(eng.fov/2)) )
				)
				+ plr.theta
			);
			
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
			
			float cellX = fmod(plr.x, 1); // Position in current cell
			float cellY = fmod(plr.y, 1);
			
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
					rayTex = tex.wallFB;
					
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
					rayTex = tex.wallLR;
					
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
			
			// Note wall texture for edge function
			frameBuf[eng.renH][r] = rayTex;
			
			// Adjust ray for aberration
			rayDist *= cos(plr.theta - rayTheta);
			
			// Loop for each phi ray
			float rayHeight = 0;
			for (int p = 0; p < eng.renH; p++)
			{
				// Find angle of ray
				float rayPhi = (
					atan(
						  ( (eng.renH / 2.0) - 0.5 - p )
						/ ( eng.renH / (2 * tan(eng.fovPhi/2)) )
					)
					+ plr.phi
				);
				
				// Find wall collision height
				rayHeight = plr.h - (rayDist / tan(rayPhi));
				
				// Write phi ray results to frame buffer
				// Ceiling
				if (rayHeight >= map.wallH)
				{
					frameBuf[p][r] = tex.ceiling;
				}
				// Floor
				else if (rayHeight <= 0)
				{
					frameBuf[p][r] = tex.floor;
				}
				// Wall
				else if (
					   rayHeight > 0
					&& rayHeight < map.wallH
				)
				{
					frameBuf[p][r] = rayTex;
				}
				// Unknown
				else
				{
					frameBuf[p][r] = tex.unknown;
				}
			}
						
			// Possibly dump rays to log
			if (input == 'd')
			{
				// Find top and bottom of columns
				int colT = 0;
				int colB = eng.renH - 1;
				for (int p = 1; p < eng.renH - 1; p++)
				{
					// Column Top
					if (
						   frameBuf[p  ][r] == rayTex
						&& frameBuf[p-1][r] == tex.ceiling
					)
					{ colT = p; }
					
					// Column Bottom
					if (
						   frameBuf[p  ][r] == rayTex
						&& frameBuf[p+1][r] == tex.floor
					)
					{ colB = p; }
				}
				
				// Print Ray info
				fprintf(
					pLog, "%d,%f,%f,%d,%d,%d,%d,%c\n"
					, r, rayTheta * RTD, rayDist, wallX, wallY
					, colT, colB, rayTex
				);
			}
		}
		
		// Draw edges
		for (int x = 0; x < eng.renW; x++)
		{
			// Wall tex for this column
			char colTex = frameBuf[eng.renH][x];
			
			// Wall tex left column
			char colTexL = colTex;
			if (x >= 1) // OOB prevention
			{ colTexL = frameBuf[eng.renH][x-1]; }
			
			// Wall tex right column
			char colTexR = colTex;
			if (x <= eng.renW - 2) // OOB prevention
			{ colTexR = frameBuf[eng.renH][x+1]; }
			
			for (int y = 0; y < eng.renH; y++)
			{
				// Ceiling trans
				if (
					   frameBuf[y  ][x  ] == tex.ceiling
					&& y + 1 < eng.renH // OOB prevention
					&& frameBuf[y+1][x  ] == colTex
				)
				{
					// OOB prevention
					if (
						   x == 0
						|| x == eng.renW - 1
					)
					{
						frameBuf[y][x] = tex.tran;
					}
					// Concave up trans
					else if (
						   frameBuf[y  ][x-1] == colTexL
						&& frameBuf[y  ][x+1] == colTexR
					)
					{
						frameBuf[y][x] = tex.tranCcu;
					}
					// Neg trans
					else if (frameBuf[y  ][x-1] == colTexL)
					{
						frameBuf[y][x] = tex.tranNeg;
					}
					// Pos trans
					else if (frameBuf[y  ][x+1] == colTexR)
					{
						frameBuf[y][x] = tex.tranPos;
					}
					// Normal case
					else
					{
						frameBuf[y][x] = tex.tran;
					}
				}
				// Floor trans
				else if (
					   frameBuf[y  ][x  ] == tex.floor
					&& y - 1 >= 0 // OOB prevention
					&& frameBuf[y-1][x  ] == colTex
				)
				{
					// OOB prevention
					if (
						   x == 0
						|| x == eng.renW - 1
					)
					{
						frameBuf[y][x] = tex.tran;
					}
					// Concave up
					else if (
						frameBuf[y-1][x+1] != colTexR
						&& (
							   frameBuf[y-1][x-1] == tex.tran
							|| frameBuf[y-1][x-1] == tex.tranNeg
						)
					)
					{
						frameBuf[y][x] = tex.tranCcu;
					}
					// Pos trans
					else if (frameBuf[y-1][x+1] != colTexR)
					{
						frameBuf[y][x] = tex.tranPos;
					}
					// Neg trans
					else if (frameBuf[y-1][x-1] != colTexL)
					{
						frameBuf[y][x] = tex.tranNeg;
					}
					// Normal case
					else
					{
						// Baseboard wall transition
						if (colTexL != colTex)
						{ frameBuf[y][x] = tex.tranWall; }
						else
						{ frameBuf[y][x] = tex.tran; }
					}
				}
			}
		}
		
		// Pile frame buffer into a single string
		//                 ((Line + \n) * #lines) + \0
		int printBufSize = ((eng.renW + 1) * eng.renH) + 1;
		char printBuf[printBufSize];
		printBuf[printBufSize-1] = '\0'; // End of string character
		
		for (int y = 0; y < eng.renH; y++)
		{
			int lineOffset = y * (eng.renW + 1);
			
			for (int x = 0; x < eng.renW; x++)
			{
				printBuf[x + lineOffset] = frameBuf[y][x];
			}
			
			printBuf[eng.renW + lineOffset] = '\n';
		}
		
		// Print frame. Only uses one printf. Much faster!
		system("cls");
		printf("%s", printBuf);
		
		// UI scenes
		// Options menu
		if (input == 'm')
		{
			// Status Bar
			printf("b = back | d = dump | r = resolution | o = open map\n> ");
			
			// Get input
			input = getch();
			switch(input)
			{
				case 'b': { break; } // Return to main screen
				case 'd': {	break; } // Enter dump screen
				case 'r': { break; } // Enter res select screen
				case 'o': { break; } // Enter res select screen
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
					&& input != 'o'
				)
				{ input = ' '; }
			}
		}
		// Open map
		else if (input == 'o')
		{
			// Status Bar
			printf("Enter \"[map name].map\"\n> ");
			
			// Read map
			char mapName[256];
			scanf("%s", mapName);
			map = readMap(mapName, &plr);
			
			// Return to main scene
			input = ' ';
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
			printf("Enter \"[console W] [console H] [font W] [font H]\"\n> " );
			
			// Get input
			int consoleW;
			int consoleH;
			scanf("%d %d %d %d", &consoleW, &consoleH, &eng.fontW, &eng.fontH);
			
			// Set render resolution
			eng.renW = consoleW - 1; // Subtract 1 column so you don't get two newlines
			eng.renH = consoleH - 2; // Subtract 2 lines for UI
			if (eng.renW < 1) { eng.renW = 1; }
			if (eng.renH < 1) { eng.renH = 1; }
			
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
				, plr.theta * RTD, 248 // Degree symbol
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
