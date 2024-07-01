
Wave enemy spawn thing first
    * Can still do 
    * Can build ui system
    * Can build “hero” system
    * Still just some a star shit and attack for enemies
Map can first just be plane
Then plane with hole/pillar etc
Then just build out map/hero pool further
Then turn into arena game
Then turn into m+ game
Create effects system which you queue things into 


# Actual ToDo 
* Get a plane up as a map
* Fix some camera things?
* Get your dude in and be able to walk around
* Write a work queue
* Write debug window of time it takes for each system

# Modelling
## Heroes/Champs
* Mage
* Warrior
* Rogue 
* Healer/Priest?

## Maps
* Be creative so that if need be you can do things like water here

# Game Design
* Figure out what heroes abilities should be
* Figure out what maps actually should look like and involve
* Figure out if you want basic ai or just multiplayer


# Game systems
## Data formats
* Skeletal animation
* One for each class?
* One for each map
* One for rendering stuff

## Networking?!?!?!
* glhf

## Texture/Asset system 
* Be able to load in every asset needed for a given map etc
    * packages?
* Actually handle texture units on load not through sending it

## Skeletal Animation
* Create your first dude with animations
* Figure out which ones you need
* Specify your own animation format

## Collision detection
* do GJK both for font engine and for collision with map?
* Easy solution is circle around the champs at least but the map should be convex
## Physics System
* Actually do 2d simulations with movement (and hack them when you don't want them properly)
* Allow for knockbacks etc 

## Actual renderer
* Actually do a render "pass" instead of just query and send things randomly

## Input handling
* Figure out better way to query/understand press/release
    * There is a difference between typing and playing   

## Audio engine
* Figure out ALSA/DirectSound

## UI system
* Use IMGUI for console/debug info, but everything else you need a system for
* Slider, Radio

## Font engine
* Be able to change font dynamically
* Be able to make the font thicker
* Be able to have an outline on it
* Do GJK so you actually can do things

## Shape editor
* Create a convex shape editor so you can create maps from it
* Question is if we can just do blender for this?


