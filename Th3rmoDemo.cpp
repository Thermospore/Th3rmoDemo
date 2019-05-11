#include <stdio.h>
#include <math.h>
#include <cstdlib>
#include <conio.h>

#define PI 3.14159265

struct player
{
	float x;
	float y;
	float theta; // Radians
	
	float speedMov;
	float speedTurn;
};

struct map
{
	bool walls[10][10];
	
	// Player starting conditions
	float startX;
	float startY;
	float startTheta; // Radians
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
};

// Takes a theta value and gives you the direction of north
char northArrow(float theta)
{
	theta *= 180/PI; // Convert to degrees
	
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

int main()
{
	// Set constants
	int dispH = 23; // H & W of display
	int dispW = 79;
	
	float fov = 90 * (PI / 180); // Convert to radians
	
	float distColMax = 0.3; // Distance at which a column will fill the whole screen
	float distColCurve = 0.2; // Affects strength of curve concavity. Range: (0,1)
	float distRend = 3; // Walls cut off at this distance
	
	// Initialize screen buffer
	char frameBuf[dispH + 1][dispW]; // Using last row to store wall tex
	for (int x = 0; x < dispW; x++)
	{
		for (int y = 0; y < dispH + 1; y++)
		{
			frameBuf[y][x] = ' ';
		}
	}
	
	// Set texture pack
	struct texturePack tex =
	{
		  '#'  //WallLR
		, 'L'  //WallFB
		, ' '  //Ceiling
		, '.'  //Floor
		, '_'  //Tran
		, '\\' //TranNeg
		, '/'  //TranPos
		, 'V'  //TranCcu
		, '^'  //TranCcd
	};
	
	// Define map
	struct map map_test =
	{
		{
			{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
			{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
			{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
			{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
			{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
			{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
			{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
			{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
			{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
			{ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
		},
		0.2, // startX
		0.3, // startY
		50 * (PI/180), // startTheta
	};
	
	// Place player in map and set attributes
	struct player plr =
	{
		map_test.startX, map_test.startY, map_test.startTheta
		, 0.1           // speedMov
		, 10 * (PI/180) // speedTurn
	};
	
	// Loop per frame
	char input = '\0';
	while (input != 'h')
	{
		// Wrap player theta
		if (plr.theta >= 2*PI) { plr.theta -= 2*PI; }
		else if (plr.theta < 0) { plr.theta += 2*PI; }
		
		// Loop for each ray
		float spacing = fov / dispW;
		float startTheta = plr.theta + (fov / 2);
		for(int r = 0; r < dispW; r++)
		{
			// Find angle of ray
			float rayTheta = startTheta - (spacing / 2) - (spacing * r);
			
			// Wrap rayTheta
			if (rayTheta >= 2*PI) { rayTheta -= 2*PI; }
			else if (rayTheta < 0) { rayTheta += 2*PI; }
						
			// Find ray length from player to wall
			char rayTex = ' ';
			float rayDist = 0;
			if ( // Right wall
				rayTheta <= atan((1 - plr.y) / (1 - plr.x))
				|| rayTheta > (3.0/2.0)*PI + atan((1 - plr.x) / (plr.y))
			)
			{
				rayDist = (1 - plr.x) / cos(rayTheta);
				rayTex = tex.WallLR;
			}
			else if ( // Forward wall
				rayTheta <= PI/2 + atan((plr.x) / (1 - plr.y))
			)
			{
				rayDist = (1 - plr.y) / sin(rayTheta);
				rayTex = tex.WallFB;
			}
			else if( // Left Wall
				rayTheta <= PI + atan((plr.y) / (plr.x))
			)
			{
				rayDist = (-plr.x) / cos(rayTheta);
				rayTex = tex.WallLR;
			}
			else // Back Wall
			{
				rayDist = (-plr.y) / sin(rayTheta);
				rayTex = tex.WallFB;
			}
			
			// Adjust ray for aberration?
			rayDist *= cos(plr.theta - rayTheta);
			
			// Calculate column height
			float colH = pow(distColCurve, rayDist - distColMax) * dispH;
			
			// Store to frame buffer
			frameBuf[dispH][r] = rayTex; // Note wall texture for edge function
			for (int y = 0; y < dispH; y++)
			{
				if ( // Wall
					y > (dispH - colH) / 2
					&& y <= colH + (dispH - colH) / 2
				)
				{ 
					if (rayDist > distRend) // Cut off wall after certain distance
					{
						frameBuf[y][r] = tex.Ceiling;
					}
					else
					{
						frameBuf[y][r] = rayTex;
					}
				}
				else if ( // Floor
					y > colH + (dispH - colH) / 2
				)
				{
					frameBuf[y][r] = tex.Floor;
				}
				else if ( // Ceiling
					y <= (dispH - colH) / 2
				)
				{
					frameBuf[y][r] = tex.Ceiling;
				}
				else // Unknown case
				{
					frameBuf[y][r] = '?';
				}
			}
			
			// Print ray info
			//printf("r%2d: Th: %8f dR %9f cH%5.2f\n", r, rayTheta, rayDist, colH);
		}
		
		// Draw edges
		for (int x = 0; x < dispW; x++)
		{
			// Wall tex for this column
			char colTexWall = frameBuf[dispH][x];
			
			// Wall tex left column
			char colTexWallL = colTexWall;
			if (x >= 1)
			{ colTexWallL = frameBuf[dispH][x-1]; }
			
			// Wall tex right column
			char colTexWallR = colTexWall;
			if (x <= dispW - 2) // Don't go out of bounds
			{ colTexWallR = frameBuf[dispH][x+1]; }
			
			for (int y = 0; y < dispH; y++)
			{
				if ( // Ceiling trans
					frameBuf[y][x] == tex.Ceiling
					&& y + 1 < dispH
					&& frameBuf[y+1][x] == colTexWall
				)
				{
					if ( // Don't go out of bounds on array
						x == 0
						|| x == dispW - 1
					)
					{
						frameBuf[y][x] = tex.Tran;
					}
					else if ( // Concave up trans
						frameBuf[y][x-1] == colTexWallL
						&& frameBuf[y][x+1] == colTexWallR
					)
					{
						frameBuf[y][x] = tex.TranCcu;
					}
					else if ( // Neg trans
						frameBuf[y][x-1] == colTexWallL
					)
					{
						frameBuf[y][x] = tex.TranNeg;
					}
					else if ( // Pos trans
						frameBuf[y][x+1] == colTexWallR
					)
					{
						frameBuf[y][x] = tex.TranPos;
					}
					else // Normal case
					{
						frameBuf[y][x] = tex.Tran;
					}
				}
				if ( // Floor trans
					frameBuf[y][x] == tex.Floor
					&& y - 1 >= 0
					&& frameBuf[y-1][x] == colTexWall
				)
				{
					if ( // Don't go out of bounds on array
						x == 0
						|| x == dispW - 1
					)
					{
						frameBuf[y][x] = tex.Tran;
					}
					else if (
						frameBuf[y-1][x+1] != colTexWallR
						&& (
							frameBuf[y-1][x-1] == tex.Tran
							|| frameBuf[y-1][x-1] == tex.TranNeg
						)
					)
					{
						frameBuf[y][x] = tex.TranCcu;
					}
					else if ( // Pos trans
						frameBuf[y-1][x+1] != colTexWallR
					)
					{
						frameBuf[y][x] = tex.TranPos;
					}
					else if ( // Neg trans
						frameBuf[y-1][x-1] != colTexWallL
					)
					{
						frameBuf[y][x] = tex.TranNeg;
					}
					else // Normal case
					{
						frameBuf[y][x] = tex.Tran;
					}
				}
			}
		}
		
		// Display buffer
		int printBufSize = ((dispW + 1) * dispH) + 1; // ((Line + \n) * #lines) + \0
		char printBuf[printBufSize];
		
		for (int y = 0; y < dispH; y++)
		{
			int lineOffset = y * (dispW + 1);
			
			for (int x = 0; x < dispW; x++)
			{
				printBuf[x + lineOffset] = frameBuf[y][x];
			}
			
			printBuf[dispW + lineOffset] = '\n';
		}
		
		system("cls");
		printf("%s", printBuf); // Only use one printf. Much faster!
		
		// Debug
		/*printf(
			"A:%5.1f | B:%5.1f | C:%5.1f | D:%5.1f\n"
			, (atan((1 - plr.y) / (1 - plr.x)))*(180/PI)
			, (PI/2 + atan((plr.x) / (1 - plr.y)))*(180/PI)
			, (PI + atan((plr.y) / (plr.x)))*(180/PI)
			, ( (3.0/2.0)*PI + atan((1 - plr.x) / (plr.y)) )*(180/PI)
		);*/
		
		// UI
		if (input == 'm') // Options menu
		{
			// Status Bar
			printf("b = back | unimplemented\n> ");
			
			// Get input
			input = getch();
			switch(input)
			{
				case 'b': { break; } // Return to main screen
			}
		}
		else if (input == 'c') // Controls
		{
			// Status Bar
			printf("h = halt (quit) | wasd = movement | q/e = look\n> ");
			
			// Get input
			input = ' '; // Prevents you from getting stuck in controls menu
			system("pause");
		}
		else // Main menu
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
				case 'h': { break; } // Will break out of while loop
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
				case 'm': { break; } // Will enter menu
				case 'c': { break; } // Will show controls
				default: { break; } // Nothing happens
			}
		}
	}
	
	return 0;
}
