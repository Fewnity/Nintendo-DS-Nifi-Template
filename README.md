# Nintendo DS Nifi Template
DS to DS Wifi (Nifi) project template

<img src="https://user-images.githubusercontent.com/39272935/184286435-062d0203-4e0b-42a4-8b5b-fa4f1571697c.png" width="800">

## Notes
Improved Nifi template of [jpenny19993 dsnifi](https://github.com/jpenny1993/dsnifi).<br><br>
The original template allow only 2 ds at the same time.<br>
The problem with the Nifi library is that you can't send data with 2 ds at the same time because some data will be lost.

I added:
- A callback system to prevent packet loss
- The possibility to have more than 2 DS at the same time
- A timeout system if the client is disconnected

There are some improvements to do but it's usable.

## Prerequisites

1. Install [devkitpro](https://devkitpro.org/wiki/Getting_Started) v3.0.3 (latest)
1. Clone [Fewnity/dswifi](https://github.com/Fewnity/dswifi)
1. put the dswifi folder in the devkitPro root folder
1. Open a terminal in the root of the repo
1. Run the `make` command to build the dswifi library
1. Run the `make install` command to replace the stock dswifi library provided with devkitpro OR copy the lib & include folders of dswifi into libnds folder of devkitPro

## Build

1. Open a terminal in the root of the repo
1. Run the `make` command to build the .nds file

## Deploy

Copy / Past the .nds file on your NDS flashcart

## Run

Launch the .nds application on some NDS consoles, tap the touch screen with your stylus and the co-ordinates of the touch event should be draw on the other NDS.
