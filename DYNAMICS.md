ately I've been playing a lot of Noida
0:03
the most recent release from NOLA games
0:05
it combines my favorite genre roguelikes
0:08
with the mechanics of popular falling
0:11
sand games like powder toy while playing
0:14
I realized that I'd never taken the time
0:16
to dive in and figure out how these
0:18
types of sand games work so with that in
0:20
mind let's explore the tech behind sand
0:22
games like Noida as I discuss how I
0:25
implemented my version of it
0:27
[Music]
0:32
for this I'm using gunslinger which is
Gunslinger
0:35
my open-source c99 framework I've been
0:37
working on for games tools and
0:39
visualizers it's portable and compiles
0:42
for Windows Mac and Linux and it comes
0:44
with a few examples for quickly getting
0:46
started I even used it to make all the
0:48
animations for this video using a really
0:50
crappy vector renderer I wrote over the
0:51
course of a few days there's a github
0:54
link in the description for anyone
0:55
interested in checking it out
0:56
in my research I was able to find
Research/Resources
0:59
multiple websites as well as a GDC talk
1:01
were one of the devs from NOLA games
1:03
Petri discusses a lot of the algorithms
1:05
surrounding how a simple sand falling
1:07
game works as well as how the data could
1:09
be structured and processed first we
Cellular Automata
1:13
need to define a few things falling sand
1:16
simulations are basically a form of
1:18
complex cellular automata you can think
1:21
of most cellular automata zazz a type of
1:23
turn-based game where you have a world
1:25
that's comprised of a grid of cells and
1:27
where each turn you modify the state of
1:30
each cell in your grid based on the
1:32
neighbors that surround that cell one of
1:35
the most classically used examples is
1:36
John Conway's Game of Life as you can
1:39
tell these rules are really simple but
1:41
incredibly complex machines and
1:43
simulations can be built out of just
1:44
using these rules alright so with that
1:48
said let's go back to Noida
Sand Algorithm
1:52
our entire game world is made up of a 2d
1:55
array of particle data each particle is
1:59
the structure containing all of the
2:00
information it needs in order to know
2:02
how to behave in the world such as its
2:04
ID total lifetime color and whether or
2:07
not it's been updated yet for this
2:08
particular frame each frame of our
2:10
simulation we iterate through the
2:12
particle data row by column for each
2:14
cell and our grid we examine its ID type
2:17
for empty cells we ignore them and for
2:20
any other IDs we'll call the update
2:21
function for that cells type so let's
2:24
take a look at a contrived example here
2:26
we have a single sand block placed
2:28
somewhere in the grid when we get to its
2:30
update loop the sand will follow this
2:32
set of rules if the block below it is
2:35
empty we'll move down otherwise if the
2:38
block below into the left is empty we'll
2:40
move down to the left otherwise if the
2:43
block below and to the right is empty
2:44
then we'll move there
2:46
and if all else fails we'll just stay
2:50
put and with just this the results are
2:53
quite interesting already and the beauty
2:56
of this is that the rules are really
2:57
simple but we can alter them and change
3:00
certain properties in order to get more
3:02
refined and realistic configurations as
3:04
we'll see later on I should also mention
3:06
as a general rule all of our particles
3:09
will be confined to the space of the
3:10
world and simply won't be able to leave
3:12
the bounds of it
3:13
[Music]
Water
3:18
water and other liquids initially behave
3:21
exactly the same as sand falling down
3:24
and looking left and right below them
3:25
for a place to move however unlike sand
3:29
when these first three rules fail they
3:32
will also look to their immediate left
3:33
and right respectively this tends to
3:35
give off the effect of flowing to
3:37
simulate the property that water will
3:39
attempt to fill any void it finds
3:40
available to it so again with these
3:45
simple rules we have a decent starting
3:47
point for sand and water but we're not
3:49
done as I mentioned earlier these rules
3:51
can be modified and adapted in order to
3:53
achieve more realistic results and up
3:56
until this point we haven't actually
3:57
been taking velocity or any other forces
4:00
into account this of course means that
4:02
all of our particles will fall at a
4:03
constant speed of one cell per frame and
4:05
the water will flow well just as slowly
4:09
let's add a constant rate of
4:11
acceleration for gravity to change our
4:13
velocity as well as for liquids a spread
4:16
factor velocity which will allow us to
4:18
spread faster left and right yeah that
4:23
looks terrible here's the issue
4:25
previously each particle could only ever
4:28
move one cell at a time so it was only
4:30
ever interested in its immediate
4:32
neighbors when we had velocity or any
4:34
other forces into the mix we're now
4:36
interested in every single particle that
4:38
our particle could interact with along
4:40
its way to its final position so looking
4:42
at this example the water there are
4:44
moments where the water is unable to
4:46
spread anywhere because it's requested
4:48
position is unavailable so it stops
4:50
completely and causes this repeated
4:52
jagged effect so here's my naive
4:56
solution to fix this using the particles
4:58
velocity will compute its desired next
5:00
position for this frame well then travel
5:03
to every cell along that path
5:06
if we run into anything along the way as
5:08
is the case here we'll stop and just
5:11
leave the particle in the last free spot
5:13
that we found
5:15
now with that change our water is able
5:17
to flow in a much more convincing manner
5:19
and our sand particles can fall at a
5:21
faster rate all right time to add in
Wood/Walls
5:25
another particle let's do something
5:27
stationary made out of wood and this
5:29
thing's pretty simple to update it won't
5:31
be affected by gravity or any other
5:33
forces however whenever the particles
5:35
start to fall you start to get some
5:37
really cool emergent behaviour
5:38
especially when you say erase parts of
5:41
the wall or when you flood various
5:45
valleys and areas and watch it spill
5:47
over into the surrounding floor I want
Fire
5:56
to jump ahead to fire it's one of my
5:58
favorite particles mainly because of all
6:00
the dynamic behaviors that just
6:01
naturally emerge from it its destructive
6:04
it only lives for a short amount of time
6:06
and in its most simple version the only
6:09
way it's able to move is by consuming
6:11
particles next to it that are flammable
6:13
this also generates a lot of smoke as
6:15
you can see from the footage smoke is
6:18
pretty simple to do if you think about
6:19
it it's really just sand but inverted so
6:22
instead of falling down due to gravity
6:24
the smoke will just rise to the top of
6:27
the world and then kind of pool up there
6:29
so at this point we've assembled kind of
Gunpowder/Salt/Lava/Oil/Acid
6:32
a nice collection of particles we have
6:34
sand water fire embers and smoke adding
6:41
new ones really should become second
6:43
nature once we have our base rules down
6:44
gunpowder for instance works almost
6:47
exactly the same as sand does however it
6:50
has a very high flammability chance
6:52
salts works exactly the same as sand as
6:55
well however it has a chance to dissolve
6:57
in water it's also less dense than water
7:00
so you can say that if it's surrounded
7:03
by water particles for instance then it
7:05
has a chance to rise to the top lava is
7:09
liquid in movement however it has a
7:11
really high chance to catch things on
7:12
fire
7:14
steam works just like smoke however it's
7:17
a byproduct of whenever fire and water
7:19
touch oil works exactly the same as
7:22
water and movement except it's very
7:24
flammable and acid works exactly the
7:28
same in movement as any other liquid but
7:30
it's very highly corrosive and will eat
7:32
through most things so I think we have
Polish/UI/Drag-Drop Images
7:37
our basic implementation down but as a
7:38
final polish let's go ahead and make
7:40
this feel like an actual tool where you
7:42
can select whatever particle you want to
7:44
paint with on the side visually
7:46
everything's pretty underwhelming so
7:48
let's add in a bit of post-processing
7:50
like some bloom and a little bit of
7:53
gamma correction
7:54
yeah that's feeling better but there's
7:56
still something not right to me see most
7:59
cellular automata is you can paint in
8:01
little States but typically you start
8:03
with a starting state and we're not
8:05
really doing that here so here's what I
8:08
want to do let's allow the user to drag
8:10
and drop images from disk into the
8:11
program then we'll take all the pixel
8:14
data from that image and we'll create
8:16
particles for each one so that we can
8:18
interact with the scene now you'll
8:20
notice most images are rich in color
8:23
they're typically 32-bit rgba and we
8:26
have a very limited color palette with
8:28
just our particles so what we'll have to
8:31
do is very simply map this rich 32-bit
8:34
RGB a data down to our small limited
8:37
palette and this is easy enough to do
8:40
we'll just look at every single pixel
8:42
and we'll match that with our pallet
8:44
range and based on at least Euclidean
8:47
distance algorithm we'll select the most
8:48
appropriate one of course there's going
8:50
to be artifacts with this but it's
8:53
simple enough for this application and I
8:55
really think it has a certain sense of
Final Sand Sim Presentation / Exploding Pictures
8:57
charm to it
8:58
[Music]
9:04
so that's it for the video I hope you
9:07
found this interesting all the source
9:10
code for this project is available
9:12
online open source I'll have a github
9:14
link down in the description I'm also
9:17
gonna put a link to my discord channel
9:18
so if you have any questions about games
9:20
graphics or programming in general feel
9:22
free to jump on there and ask me or any
9:24
one of the other devs that's on there
9:26
and I'm kind of treating this as a
9:28
launchpad for a new series where I'd
9:30
like to look at mechanics especially
9:32
from popular video games and dissect
9:34
those and kind of do my own
9:35
implementation so if you have any
9:38
suggestions for what you'd like to see
9:40
please let me know in the comments or in
9:42
the discord channel I'd love to hear
9:43
from you anyway thanks for watching and
9:47
I hope to see you in the next one
10:00
you

extract the proper algorithsm I need in their entierity
