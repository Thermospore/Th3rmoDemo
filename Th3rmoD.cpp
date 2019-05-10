#include <stdio.h>
#include <math.h>
#include <cstdlib>
#define PI 3.14159265

struct player
{
	float posX;
	float posY;
	float theta; // Radians
};

struct map
{
	bool walls[10][10];
	
	// Player starting conditions
	float startX;
	float startY;
	float startTheta; // Radians
};

int main()
{
	// Set constants
	int dispH = 23; // H & W of display
	int dispW = 79;
	
	float fov = 90 * (PI / 180); // Convert to radians
	
	float distColMax = 0.3; // Distance at which a column will fill the whole screen
	float distColCurve = 0.2; // Affects strength of curve concavity. Range: (0,1)
	float distRend = 3; // Walls cut off at this distance
	
	float speedMov = 0.1;
	float speedTurn = 10 * (PI/180);
	
	char texWallLR  = '#';
	char texWallFB  = 'L';
	char texCeiling = ' ';
	char texFloor   = '.';
	char texTran    = '_';
	char texTranNeg = '\\';
	char texTranPos = '/';
	char texTranCcu = 'V';
	char texTranCcd = '^';
	
	// Initialize screen buffer
	char screenBuffer[dispH+1][dispW]; // Using last row to store wall tex
	for (int x = 0; x < dispW; x++)
	{
		for (int y = 0; y < dispH + 1; y++)
		{
			screenBuffer[y][x] = ' ';
		}
	}
	
	// Define map
	struct map donut =
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
	
	// Place player in map
	struct player thermo = { donut.startX, donut.startY, donut.startTheta }; 
	
	// Loop per frame
	char input = '\0';
	while (input != 'h')
	{
		// Wrap player theta
		if (thermo.theta >= 2*PI)
		{
			thermo.theta -= 2*PI;
		}
		else if (thermo.theta < 0)
		{
			thermo.theta += 2*PI;
		}
		
		// Loop for each ray
		float spacing = fov / dispW;
		float startTheta = thermo.theta + (fov / 2);
		for(int r = 0; r < dispW; r++)
		{
			// Find angle of ray
			float rayTheta = startTheta - (spacing / 2) - (spacing * r);
			
			// Loop rayTheta back around
			if (rayTheta >= 2*PI)
			{
				rayTheta -= 2*PI;
			}
			else if (rayTheta < 0)
			{
				rayTheta += 2*PI;
			}
			
			// Find ray length from player to wall
			char rayTex = ' ';
			float rayDist = 0;
			if ( // Right wall
				rayTheta <= atan((1 - thermo.posY) / (1 - thermo.posX))
				|| rayTheta >= 2*PI - atan((1 - thermo.posY) / (thermo.posX))
			)
			{
				rayDist = (1 - thermo.posX) / cos(rayTheta);
				rayTex = texWallLR;
			}
			else if ( // Forward wall
				rayTheta <= PI - atan((1 - thermo.posY) / (thermo.posX))
			)
			{
				rayDist = (1 - thermo.posY) / sin(rayTheta);
				rayTex = texWallFB;
			}
			else if( // Left Wall
				rayTheta <= PI + atan((thermo.posY) / (thermo.posX))
			)
			{
				rayDist = (-thermo.posX) / cos(rayTheta);
				rayTex = texWallLR;
			}
			else // Back Wall
			{
				rayDist = (-thermo.posY) / sin(rayTheta);
				rayTex = texWallFB;
			}
			
			// Adjust ray for aberration?
			rayDist *= cos(thermo.theta - rayTheta);
			
			// Calculate column height
			float colH = pow(distColCurve, rayDist - distColMax) * dispH;
			
			// Store to screen buffer
			screenBuffer[dispH][r] = rayTex; // Note wall texture for edge function
			for (int y = 0; y < dispH; y++)
			{
				if ( // Wall
					y > (dispH - colH) / 2
					&& y <= colH + (dispH - colH) / 2
				)
				{ 
					if (rayDist > distRend) // Cut off wall after certain distance
					{
						screenBuffer[y][r] = texCeiling;
					}
					else
					{
						screenBuffer[y][r] = rayTex;
					}
				}
				else if ( // Floor
					y > colH + (dispH - colH) / 2
				)
				{
					screenBuffer[y][r] = texFloor;
				}
				else if ( // Ceiling
					y <= (dispH - colH) / 2
				)
				{
					screenBuffer[y][r] = texCeiling;
				}
				else // Unknown case
				{
					screenBuffer[y][r] = '?';
				}
			}
			
			// Print ray info
			//printf("r%2d: Th: %8f dR %9f cH%5.2f\n", r, rayTheta, rayDist, colH);
		}
		
		// Draw edges
		for (int x = 0; x < dispW; x++)
		{
			// Wall tex for this column
			char texWall = screenBuffer[dispH][x];
			
			// Wall tex left column
			char texWallL = texWall;
			if (x >= 1)
			{
				texWallL = screenBuffer[dispH][x-1];
			}
			
			// Wall tex right column
			char texWallR = texWall;
			if (x <= dispW - 2) // Don't go out of bounds
			{
				texWallR = screenBuffer[dispH][x+1];
			}
			
			for (int y = 0; y < dispH; y++)
			{
				if ( // Ceiling trans
					screenBuffer[y][x] == texCeiling
					&& y + 1 < dispH
					&& screenBuffer[y+1][x] == texWall
				)
				{
					if ( // Don't go out of bounds on array
						x == 0
						|| x == dispW - 1
					)
					{
						screenBuffer[y][x] = texTran;
					}
					else if ( // Concave up trans
						screenBuffer[y][x-1] == texWallL
						&& screenBuffer[y][x+1] == texWallR
					)
					{
						screenBuffer[y][x] = texTranCcu;
					}
					else if ( // Neg trans
						screenBuffer[y][x-1] == texWallL
					)
					{
						screenBuffer[y][x] = texTranNeg;
					}
					else if ( // Pos trans
						screenBuffer[y][x+1] == texWallR
					)
					{
						screenBuffer[y][x] = texTranPos;
					}
					else // Normal case
					{
						screenBuffer[y][x] = texTran;
					}
				}
				if ( // Floor trans
					screenBuffer[y][x] == texFloor
					&& y - 1 >= 0
					&& screenBuffer[y-1][x] == texWall
				)
				{
					if ( // Don't go out of bounds on array
						x == 0
						|| x == dispW - 1
					)
					{
						screenBuffer[y][x] = texTran;
					}
					else if ( // Pos trans
						screenBuffer[y-1][x+1] != texWallR
					)
					{
						screenBuffer[y][x] = texTranPos;
					}
					else if ( // Neg trans
						screenBuffer[y-1][x-1] != texWallL
					)
					{
						screenBuffer[y][x] = texTranNeg;
					}
					else // Normal case
					{
						screenBuffer[y][x] = texTran;
					}
				}
			}
		}
		
		// Display buffer
		system("cls");
		for (int y = 0; y < dispH; y++)
		{
			for (int x = 0; x < dispW; x++)
			{
				printf("%c", screenBuffer[y][x]);
			}
			printf("\n");
		}
		
		// UI
		if (input == 'o') // Options menu
		{
			printf
			(
				"b = back | \n> "
				, thermo.posX, thermo.posY, thermo.theta * (180/PI), 248 // Degree symbol
			);
			
			// Get input
			scanf(" %c", &input);
			switch(input)
			case 'b': { break; } // Return to main menu
		}
		else // Main menu
		{
			printf
			(
				"pos:(%1.2f, %1.2f) theta:%3.0f%c | h = quit | o = options | wasd = move | q/e = look\n> "
				, thermo.posX, thermo.posY, thermo.theta * (180/PI), 248 // Degree symbol
			);
			
			// Get input
			scanf(" %c", &input);
			switch(input)
			{
				case 'h': { break; } // Will break out of while loop
				case 'w':
				{
					thermo.posX += speedMov * cos(thermo.theta);
					thermo.posY += speedMov * sin(thermo.theta);
					break;
				}
				case 'a':
				{
					thermo.posX -= speedMov * sin(thermo.theta);
					thermo.posY += speedMov * cos(thermo.theta);
					break;
				}
				case 's':
				{
					thermo.posX -= speedMov * cos(thermo.theta);
					thermo.posY -= speedMov * sin(thermo.theta);
					break;
				}
				case 'd':
				{
					thermo.posX += speedMov * sin(thermo.theta);
					thermo.posY -= speedMov * cos(thermo.theta);
					break;
				}
				case 'q': { thermo.theta += speedTurn; break; }
				case 'e': { thermo.theta -= speedTurn; break; }
				case 'o': { break; } // Will enter settings on next loop
				default: { break; } // Nothing happens
			}
		}
	}
	
	return 0;
}
