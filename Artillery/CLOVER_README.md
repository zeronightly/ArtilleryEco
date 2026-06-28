# Clover: Inventory, Groups, and Queries

Turns out, doing these fast and in a concurrent safe way is actually pretty painful and ends up being basically the same problem.
Hi! If you're looking at clover, you're probably (hopefully) trying to either do fast queries that allow you to compose behaviors
  or looking to do, you know, inventory. The secret is that we don't really see a difference between those two things. This is a 
  design philosophy inherited from Destiny 2, where it proved pretty essential in, ya know, actually making the game. And that did
  make about a billion dollars and live for a long time. I don't want to appeal to authority though. So let's talk about what this
  does and you can make your own call. First, what it's not:
  
  This is not a complete inventory system. I'm not sure a fully general one is possible.
  It's not something we wanted to include in the core of artillery, because it is a little out of scope.
  Finally, this is headless - there's no UI included in clover. Eventually, a lot of it will appear in sunflower!
  
  What it is:
  A set of powerful and expressive primitives for describing any inventory system.
  Enough actual meat that building most inventory systems with it will be pretty simple.
  A basic Model Query Binding design where the model is an impure ECS, that is to say, Artillery
  The query is set intersections. sets are entities themself, and referenced by key.
  Sets have a subtype marker, unlike most other keys in our system.
  So a vendor is a set, an inventory is a set, the enemies that can spawn in a level would be a set.
  Sets have three kinds of members: 
 		1) regular keys like you've seen everywhere else! These are used as behavior binders! 
 		for example, you can get a state tree this way! These tend to be things you want to be able to reference quickly.
 		And aren't always what the set actually stores. When they are, they just go in the set. When they're properties of a set, they go in...
 		2) Sockets! Sockets hold keys. They allow one-to-many relationships, which is something we intentionally do not support seriously with our relationship keys.
 		Where relationship keys should be thought of a noun-verb-noun triples, or facts, sockets cover all the bullshit that comes up in games all the time.
 		like when some fucker puts on three rings instead of two, because someone made a hand of glory item? well, the hand of glory item just adds a third ring socket.
 		That's not something that describes well with the triples. you can do it, but it's awkward, and as someone with a decade of experience using graph DBs:
 		paradigm purity can fuck itself.
 		3) Item Instance keys! These are special keys that contain a reference to their item archetype uniquely in their actual key layout!
 		In simple systems, they may also contain a set reference! Sockets actually do, and it's very useful. This isn't a great fit for everything,
 		so we don't enforce or assume it. (It also makes almost the whole key just the set and instance. This is Not Great for technical reasons.)
 		Instance keys and variations on this hierarchical key concept form the backbone of what allows clover to answer certain kinds of queries in
 		literally constant time. We don't use lookup tables for a lot of relationships. They're just encoded in the key.
 		
 		Here are the actual bits you'll see built out:
 		Vendor Sets
 		Inventory Sets
 		Results Sets
 		
 		Item Archetype Keys
 		Item Instance Keys
 		Attribute-keys-as-currency
 		
 		Sockets. So many sockets:
 		In destiny 2, there was a privileged kind of item that specifically went into sockets called plugs. 
 		We uh... just let you put whatever in there. So you might see _a lot_ of sockets show up in your design.
 		You'll likely use those or ConservedAttributeKeys to connect guns (Abilities) to things.