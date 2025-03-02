Intro
0:02
[Music] we're going to be talking about exploring the tech and design of noitan
0:08
because the game is not out yet i'm just going to show you a trailer and hopefully it'll explain a little bit
0:13
about the game we had some trouble getting the audio working so it's not again this is the curse of mine
0:38
[Music]
1:01
[Music] so uh thank you
Overview
1:13
this is essentially us uh there's three of us and then this is our uh audio department here and uh
1:21
so the talk is going to be divided into two parts and the first part we're going to talk about the tech stuff
1:27
and then it's going to be the design stuff and i'm just going to say right now that the design part is
1:33
there's not probably going to be enough time to go into too much detail about it so it's going to be a bit more
1:39
rambly but for the tech so we have a custom engine
Falling Everything
1:45
that's called falling everything and you can kind of see from the trailer we can do sand
1:51
and we can do liquids of all kinds and then we can do gases as well
1:59
and and for the last thing is like we also have like rigid bodies in there and essentially i'm going to
2:05
talk to how we've sort of done all of that and how that is sort of being accomplished
Quick Basic
2:11
and we're going to start at the basics by traveling back into a much better time period into the
2:18
90s when you could type into a text editor and characters would appear immediately
2:26
so back back in the 90s i got started in quick basic and i discovered in quick basic that
2:32
you can do this thing where you can put a pixel under the screen by pset and you can ask for a color of a
2:38
pixel by point by so you point at something and you ask what the color of that is
2:43
and with this amazing technology i was able to essentially write a sand simulation so
2:51
what's what's happening here is uh it's testing that if there's a sand pixel here
2:56
it's looking at the line below it if that's empty then move down and then
3:02
if that's occupied it looks uh to the left and the right to the down left and the right and it moves in that
3:08
direction and with that you essentially get this which is a rudimentary sand simulation and this
3:15
is like 95 percent of our tech is this
3:21
then i really quickly discovered that you can also do water and it's essentially the same
3:27
algorithm but at the very end of that you also like check to the left and to the right and if they're empty you move
3:32
in that direction so with that all of it sort of settles down and now you've got water and sand and then the gases
3:40
are just like inverse of this so that it's not that complicated it's now
3:46
you all know if you've been wondering um but we're travel
3:51
forward in time uh about 12 years and i'm using visual c plus plus 6.0 and i do not know how
3:58
good my life is and i ended up making this game called bloody zombies
Bloody Zombies
4:04
and this was made in 2008 and it was made for gamma 256 and it was a competition where you had
4:11
to make a game that used resolution 256 times 256 or lower so
4:18
while i was working on this this is essentially using the same algorithm as in the quick basic one
4:25
but what i discovered is you can also like make it a bit more liquid-like i'll show
4:32
you more gifs of or gifs so you can make it more liquid-like by adding another sort of a simulation
4:39
which is like this particle simulation and so the thing that's happening in in this like when the player jumps into the
4:45
blood and it splatters all around what's happening is taking one of those pixels out of that falling sand simulation
4:52
and it's putting it into a separate particle simulation and it's tracking
4:58
its velocity and gravity in there and it's traveling in that until it hits another pixel in the world
5:04
and then it gets put back into the falling sand simulation right so with this you can make things a
5:11
lot more liquid-like and not just like this blobby thing that's falling down and we're using this technique still to
5:17
this day in noita so after i was done with
Crayon Physics
5:24
bloody zombies which was kind of like a quick game i made crayon physics and then
5:31
after that i was interested in like can i add rigid bodies to the simulation
5:37
i was using visual studio c plus plus 2008 and it turns out you can add rigid
Rigid Bodies
5:45
bodies to this but it's a bit more complicated
5:50
and essentially the way this works is i'm using box 2d for the rigid body
5:56
stuff and it's sort of like integrated with this falling sand simulation
Adding Rigid Bodies
6:03
so the way it works is you have all the pixels that belong to a rigid body
6:09
they know their material and they know they want to be one sort of rigid body and what you do with all those pixels is
6:14
you apply a marching square algorithm to it and that marching squares algorithm essentially produces
6:21
this outline of all where all the pixels are and that's a lot of vertices so what you
6:28
do then is you give it a document specker algorithm and that's essentially the smooths that out so you
6:35
get a lot less vertices and once you haven't like to smooth it out
6:43
mesh you give it into a triangulation algorithm and you get a bunch of triangles and at this point
6:49
you're at a stage where you can just give those triangles into box 2d and in box 2d can sort of simulate those
6:57
bodies and the sort of final step to this is that every pixel
7:03
that's here it knows its uv coordinates inside of that
7:09
triangle and it knows which body it belongs to so that's the way it can kind of like
7:15
figure out its new position and and the way you put these two
7:21
simulations together so you have two different simulations at this stage the way you put them together is
Simulations
7:28
at the very beginning of the frame you take your rigid body pixels out of the world
7:36
then you run your falling science simulation one step and you run your box 2d one step and then you put your pixels rigid body
7:43
pixels back into the world and they get their updated positions from box 2d right and
7:51
there might be this case where there's a pixel now in the way of a rigid body and what you do in that
7:57
case you just take that pixel out and you put it into that bloody zombies particle system and you throw it into
8:03
the air so what ends up happening with that is is you get these you know splashes
8:09
like well that's not a good example let me give you a better example here so like when this this body hits the
8:16
water all those water pixels get thrown into the world so that's pretty much pretty much it
Static Stuff
8:24
we have to do a bit more to it so like one thing is like if one of these pixels
8:30
gets destroyed you have to recalculate all of the marching squares and the box 2d body
8:36
stuff and whatnot and for the rest of the world wherever there's like study
8:42
static i'm using static and air quotes because you know there's nothing really static in our game uh
8:48
static stuff uses two the first two steps of that algorithm so you do
8:55
marching squares and douglas picker and then you just make these hollow bodies of the world and you only have to do
9:02
that in places where there are rigid bodies or places where they're going to be rigid
9:08
bodies and that saves you a lot of so that's pretty much the tech
9:19
there is one more step to it and this is well there's two more steps to it so
Visual Studio 2013
9:26
we're getting to visual studio 2013 and i have here a video of it taking eight minutes to boot up
9:34
and i'm making all of you suffer through this because i've had to suffer through it so
9:41
sort of the last step of this process of making the tech of noita was that
9:47
i had it running in this 256 times 256 area but it's really hard to
9:53
make good gameplay in like 256 times 256. so the last step was like figuring out
10:00
how to make like a really big world we can sort of have everything sort of simulated
10:06
and the problem was that it was just like taking so much cpu time to test all the pixels
10:11
that it was getting really complicated to do that so the solution to this was to figure out
10:17
how to multi-tread the sort of falling science simulation
10:22
and essentially the way it's done is
10:29
the sort of world is divided into these 64 times 64 chunks
10:37
and each of those chunks keeps a dirty rect of things that it needs to
10:43
update so you can see here the dirty wrecks that are getting updated
10:48
and what this alone like even if you don't multithread this what this alone does is like it removes
10:55
like so many of the pixels that you have to test because very often the world ends up in some
11:02
sort of semi-stable state so then you don't have to update that much stuff
11:08
but you can also multithread this pretty easily
Multithreading Problem
11:13
and the problem with multithreading a simulation like this is because it's using the same buffer so
11:19
there's no not like two buffers uh you have to make sure that another tread is not updating the
11:26
same pixel as you're updating because if there's like two threads trying to update the same pixel
11:33
all hell breaks loose and everything is destroyed
Multithreading Solution
11:38
so the way to do this is you or the way we did it is uh we update the
11:44
world in this sort of a checker pattern fashion
11:49
so at the we do four update rounds of the world so at the very beginning you gather
11:57
all the things that have to be updated and then you pick like in this case we're picking these
12:02
white ones here and what it allows things to do
12:08
is this guarantees that any pixel that's getting updated in this can go 32 pixels in any direction
12:16
and it's guaranteed that another tread won't update it so like you can see here this green area
12:23
are all the pixels where it can kind of like travel to and so the multitrading is essentially
12:29
like you take sort of every fourth chunk and you throw
12:34
them into a tread pool then you wait for all of those to be calculated then you do the next set of four
12:42
64 64 areas you throw those into a thread pool you wait until they're done uh you do it
12:48
once more and then you do it once more and then you've essentially
12:54
managed to multithread the simulation and get a lot more out of your cpus
Continuous Big World
13:01
uh the real final bit because like we have a sort of a continuous big world it's
13:09
probably the most standard technique of this which is essentially just streaming so
13:14
in this this yellow rectangle bit here represents the screen and all of these other
13:23
rectangulars are uh 512 512 areas of the world so as you sort of
13:30
travel around in the world the procedural generation system is generating new worlds as you're going
13:36
and we try to keep 12 of these in there at the same time
13:41
and then we just like take the furthest one and we write it to the disk
13:47
and then you read it back if you ever go there and this all of this sort of creates this uh very
13:53
nice uh feeling that the sort of whole world is being simulated all the time even though we're kind of
13:59
like cleverly figuring out what needs to be updated
Game Design
14:06
so that was the tech and now we're off into the design
14:17
i just want to say that the tech part is for for us it was or for me it's been
14:23
the sort of easier thing to work on and it's a game design that has been more complicated and difficult and the
14:28
reason for that is that for the tech stuff you can ask these falsifiable questions like you
14:36
can ask does this look like water is it running at 60 frames a second yes it's kind of done
14:42
but when you come to game design you have all these questions like is this game fun and it's like well depends who's asking
14:49
the question who's who's answering like i enjoy kicking ragdolls around for like eight hours a day
14:55
but like i don't know if anyone else is going to do that enjoy that so for us the real question is like
15:02
hopefully the tech was kind of cool and impressive but does it produce interesting gameplay
Emerging Gameplay
15:09
and the answer to that is i mean the game's not out so we don't quite
15:14
know but the answer to this is very complicated uh
15:21
so the naive answer to this question does it produce interesting gameplay and this is the position i had when i sort
15:26
of started this is that of course this is going to produce all this interesting gameplay it's going to work as like this
15:33
perpetual motion machine of emerging gameplay and i was kind of right this physics
15:38
engine works in that way that there's a lot of emergent stuff that happens but what i did not realize is that
15:46
emerging gameplay comes in two colors you've got the cool and interesting thing that everyone
15:51
is always hyped about and you think when you hear the words emerging gameplay but it also comes in this game breaking
15:58
fashion that can just like completely devastate your game
16:03
so this is the early prototype i did and it's the idea is that you have like this 2d building and what i didn't realize is like if you
16:10
simulate all of that it's going to end up as like this pile of rubble at the bottom of the screen
16:16
and it was this is kind of cool in a way but like how are you going to build a game out of this
16:21
when all your level design is just gonna it's gonna end up as a rubble and you can imagine like building a game
16:26
on top of a system like this it's gonna be like nightmare uh
16:31
so really the process of making noita has really been a process
Making Noita
16:38
of sort of trial and error and a process of sort of trying to tame
16:44
this emergent chaos beast that is our physics engine
16:51
and to find the game that sort of like maximizes the good emergence stuff while sort of minimizing the
16:57
bad ones and the way we've approached this is just testing a lot of things so we've tested a bunch of different games we've
17:04
tested a lot of different things and it's because it's so hard to know like if you put something in there is it the
17:10
coolest thing or the thing that's going to break the game [Music]
17:16
uh so besides testing a bunch of different games
17:22
uh what's ended up happening often is we've made changes and then we've sort of ended at a sort
Local Maximum
17:28
of local maxima so local maximum is sort of a place where the game was kind of fun but we felt that there could be
17:36
something much better out there and we didn't but we didn't know what what we should be doing to get there so
17:42
as an example of that sort of a thing
17:49
this is our earlier version of of the game and the player had this tool where they
17:54
could like dig through everything this drill
18:00
and it was kind of fun you'd get this at the very beginning of the game and it was kind of fun and interesting
18:05
to go around the world using this this tool but the problem with it
18:11
was that it essentially broke all the combat gameplay because like what you would do
18:17
is you drill this hole into a wall and you shoot all the enemies through that tiny hole
18:23
and then it was it was kind of fun but it was also kind of like breaking the
18:28
whole game uh combat gameplay so what we ended up doing at some point
18:34
was just like taking the drill out completely and what ended up happening there for a while
18:42
was that the game got a lot worse because like the whole procedural
18:47
generation system was built with that in mind that he would go through everything and it was kind of worse for a while
18:54
until we actually fixed everything and then we were actually at a better position than we started
19:01
[Music] and sort of besides being stuck at these sort of local maximas which happens very
19:07
often we few times sort of managed to push the game forward in a big splash into something better
19:15
and one of those things was when we decided to sort of make it roguelike which means it's a permadeath
Randomness
19:23
game with procedural generation uh
19:31
what really happened when we sort of made a rope light was that we managed to sort of shift this sort of
19:37
like we have this emerging chaos beast that is our physics engine we're sort of shifting uh
19:43
dealing with it first into the player and then b to the sort of random nature
19:48
of the game so like because the world's out there to kill you you as a player have to pay attention to it
19:55
and sort of be because of the random nature of the game uh we we got to put back some stuff so
20:03
for example the drill that i mentioned earlier that broke our previous game that wasn't
20:09
roguelike it has made its comeback in in noita
20:14
and because the game is random you as a player you can't like build all your strategies around just
20:21
like finding this one tool and beating the game with it because it's not that good it can sort
20:26
of go back into the game and be there and it's actually kind of fun as a player like
20:32
now as a player like finding this drill
20:38
and then figuring out how to use it and then figuring out that you can kind of break a bit of the combat if you use
20:43
that that's like an exciting thing like you think you're really clever and you found something that the designers don't know and it's kind of like an exciting
20:49
moment but we've gotten away with a lot of that stuff because because of the random
20:55
nature uh another thing making it permanent really fixed us
New Worlds
21:01
for us is uh you can essentially get a new world every time you play so previously it used to be uh
21:10
it used to be sort of a similar game but you it would be like like in terraria that when you died your
21:16
stuff would remain in the world and you could go pick that up but the problem with that was because we have such a
21:21
highly dynamic world he's a player you could like let's say you have to get into this
21:26
portal and you fill out this place with lava and it's a persistent world
21:32
then you sort of essentially screw to yourself you can't never make it there anymore
21:39
but because we we have a permadeath game we don't really care about that that much so like you if you as a player you're
21:45
kind of like dumb enough to block your progress in the game that's that's on you
21:50
you get a new chance when you die and hopefully you'll die very quickly
21:56
or very likely you're gonna uh
Roguelite
22:02
so i'm going to talk about a few more things hopefully we have time so the this is probably the
22:09
thing that's most obvious to people is like when we made it a roguelite we managed to make it like we can make it a challenging
22:15
game and the benefit of that for us is in a moment the benefit of that for us
22:22
is [Music] you as a player you really have to pay attention to the world
22:27
and the physics and how it works because the physics are there out to
22:33
kill you and related note to this was one of the
22:39
prototypes we did you played as a sort of god like entity and you could like spam lightning everywhere and kill
22:45
everyone and it was really fun for about 20 minutes like being the
22:51
super powerful entity but the problem with this was that you
22:58
wouldn't essentially pay that much attention to the physics of the world like the physics simulation turned into
23:03
sort of a visual effect for you instead of being a thing that you had to pay attention to
23:09
but now making this a roguelite has sort of allowed us to uh have our cake and eat it too so at
23:15
the very beginning of the game you sort of as a player have to pay a lot of attention to the world but if you make it far enough there you
23:21
can become like super powerful and you just like destroy everything in this one uh and the reason we did not like figure
Fixing Glitches
23:29
out that we should make this a roguelite and permanent game at the very
23:34
beginning actually i think we had a very good reason for that and the
23:41
idea behind that was that it's going to be super annoying playing this game and you're going to get killed because
23:46
of like a physics glitch or some sort of physics thing that's going to kill you and you don't understand what's happening
23:53
and and sort of our fix to that has been to try and fix as many of the glitches
23:59
as possible or make it so that if there's a glitch that we haven't figured out how to
24:05
fix that it's not going to kill you so like right now the rigid bodies
24:11
don't damage the player but they damage the enemies in there and the reason for that is we haven't
24:17
like figured out that maybe sometimes the rigid bodies can get kind of like wobbly in a place and
24:22
there's going to be huge forces that would kill the player immediately so we're just like let's just disable that that seems to work decently
24:30
it'll probably make its comeback later [Music]
Communication
24:35
but the other part of that which which i have realized during this process is
24:42
that it's really about communication to the player
24:48
so if the player dies because of something like there's lava that drops out of nowhere
24:55
they're going to be super they might be super pissed at the game if they think that's like a glitch or some random thing
25:00
but if the player notices that there's like a wooden plank here and there's lava on top of that
25:05
and they walk underneath there and then the wooden blank sets on fire and the lava drops on them
25:11
then if the player sees that they're going to blame themselves for it they're going to be like oh i wasn't careful enough like
25:18
there was this thing and really the difference between someone like hating your primitive game
25:23
and sort of blaming themselves for it really is almost i think communication of like of these
25:29
emerging systems and if you look at some classic games like if you look at net
25:36
hack net hack is actually really good at communicating what's happening
25:42
in in there like it stops the game stops and there's a line on top of the screen that says
25:47
that actually says what's happening in the world and there's a funny thing like when you
25:52
add the sort of communication thing there that happens is you as a player sort of
25:58
attribute more to the emergent system than actually might even be in there so
26:06
we had a sort of a thing where you would get stained by the liquids that you walk
26:12
through so for example uh if you got covered in
26:18
blood you'd be red and whatnot and it was purely visual at some point
26:23
and then we added like let's make it so that if you're covered in oil you're more likely to ignite
26:29
and that was kind of confusing to people like they didn't realize that okay
26:34
they're going to get get ignited so the last thing we just did was we added these
26:40
ui icons on top of you as a player that indicate which liquid you're covered in so the
26:45
blue one here indicates you're wet and as soon as that went in it went from
26:50
being like this obscure thing that no one got into a thing where people started attributing things to this so like they
26:57
said oh i'm wet now so i'll probably take more damage from being electrocuted and that wasn't implemented in there but
27:03
everyone sort of assumed that that's the case and to demonstrate like as an anecdote
27:10
how powerful this thing was we had like when you're wet now i think you take more damage from being
27:16
electrocuted but when you're wet you're not as easily you don't ignite as easily and that was actually implemented in there
27:22
and then we broke it at some point and for like two months it was not working
27:28
but we all sort of assumed that it's working there and we're just like avoiding it because it's like it's
27:33
communicating this thing and i think like some games like dwarf fortress and the sims for example
27:39
managed to like rise above to a certain level where players actually attribute more to the emerging systems
27:45
that actually are in there but okay that's that's about all the
Questions
27:50
things that i have i think we have like few minutes for questions we have one question or two two minutes
27:56
okay thank you
28:01
[Applause] hello hey um i'm curious uh you
28:07
mentioned that uh the simulation was single buffered everything happened on the same buffer yes is there a reason or did you
28:14
try double buffering uh well originally it was just that's
28:19
the way it got going uh but later on i realized like you can't like
28:25
if you do a double buffer then you have to actually update everything right unless you sort of double buffer
28:31
every junk separately but then you have to sort of maintain two different buffers
28:37
and and for i don't think there's different kinds of simulations that you can do if you do double buffered uh falling sand stuff
28:45
is much more harder i think because then you have to figure out where does this pixel have to go
28:50
because right now pixels can only occupy one place cool thanks
29:00
great talk thank you um thank you i wanted to ask about uh the procedural level generation like how did you get that part going for you
29:09
okay uh as would so i forgot to mention that we've been
29:16
working on this game for like seven six years now so we've tried a lot of
29:21
things and for procedural generation we've tried a few different things and
29:27
there are a few different things in the game right now but the main part of it is using sean barrett's
29:34
herringbone wang tile set so essentially the
29:42
what it is is like if everyone here knows how spelunky does procedural generation so spelunky does like four times four uh
29:49
squares and those squares can be connected with certain rules uh
29:56
the herringbone wang tile set is essentially uh do we have anything happen how can i
30:04
i'll demonstrate it with these things so it's it's like this sort of like a brick shape and then
30:11
you put another brick shape that goes down like this and then one that goes like this and one
30:16
that goes like this and there are certain rules like how those can be connected and the benefit of that to like the
30:23
splunky style which is like squares is that you don't see the seam as easily
30:29
because like there's not a seam that goes throughout the whole world where you see like oh this is the part where
30:34
we are on this grid and this is the part where around the screen
30:43
do we have are we done okay if there are more questions i think
30:50
there's some sort of a place out there and i can and hopefully answer those thank you so much for everyone
