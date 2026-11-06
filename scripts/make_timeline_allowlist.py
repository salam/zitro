# Curated input for make_timeline_data.py. A plain Python literal: no code runs from this file.
#
# Every event is named by its English Wikipedia title, which Wikidata resolves to one item. A wrong
# title comes back missing, so it can't silently become a different thing.
#   wiki:   English Wikipedia title of the event
#   expect: the year a person expects. Only a cross-check: the date shown always comes from Wikidata,
#           and if the two disagree the event is rejected.
#   prop:   optional: the one Wikidata date property to use for this event, where the default order
#           would pick a different moment of it (a treaty's coming into force instead of its signing)
#   image:  exact Wikimedia Commons file name, or None for no picture. Hand-chosen and looked at.
#           Never taken from Wikidata automatically: a war's main image can be a battle scene.
#   author: optional: the maker, copied from the Commons file page, where its metadata names nobody
#   tag:    what the picture shows. War and conflict topics (sensitive) allow only safe_tags.
#   fit:    optional. "cover" fills the band and crops; "contain" shows the whole picture.
#           By default maps, flags, portraits, documents and artworks are shown whole.
{
    "safe_tags": ["portrait", "map", "vehicle", "flag", "building", "document", "artwork"],
    "topics": [
        {
            "id": "world_wars",
            "subject": "World war",
            "sensitive": True,
            "events": [
                {"wiki": "Assassination of Archduke Franz Ferdinand", "expect": 1914, "image": "Ferdinand Schmutzer - Franz Ferdinand von \u00d6sterreich-Este, um 1914.jpg", "tag": "portrait"},
                {"wiki": "World War I", "expect": 1914, "image": "Map Europe alliances 1914-de.svg", "tag": "map"},
                {"wiki": "Armistice of 11 November 1918", "expect": 1918, "image": "Foch train.jpg", "tag": "vehicle"},
                {"wiki": "Treaty of Versailles", "expect": 1919, "prop": "P585", "tag": "building",
                 "image": "Vue aérienne du domaine de Versailles par ToucanWings - Creative Commons By Sa 3.0 - 083.jpg"},
                {"wiki": "World War II", "expect": 1939, "image": "Second world war europe 1935-1939 map de.png", "tag": "map", "author": "San Jose"},
                {"wiki": "Attack on Pearl Harbor", "expect": 1941, "image": "USS Arizona Memorial (aerial view).jpg", "tag": "building"},
                {"wiki": "Normandy landings", "expect": 1944, "image": "LCVP-plan.gif", "tag": "vehicle", "fit": "contain"},
                {"wiki": "United Nations", "expect": 1945, "image": "2024-11-18-Headquarters of the United Nations-0605.jpg", "tag": "building"},
            ],
        },
        {
            "id": "space_exploration",
            "subject": "Space exploration",
            "events": [
                {"wiki": "Sputnik 1", "expect": 1957, "image": "Sputnik 1 satellite model.png", "tag": "spacecraft", "fit": "contain"},
                {"wiki": "Vostok 1", "expect": 1961, "image": "Vostok spacecraft.jpg", "tag": "spacecraft", "fit": "contain"},
                {"wiki": "Apollo 11", "expect": 1969, "image": "Apollo 11 Crew.jpg", "tag": "portrait", "fit": "contain"},
                {"wiki": "Voyager 1", "expect": 1977, "image": "Voyager spacecraft.jpg", "tag": "spacecraft", "fit": "contain"},
                {"wiki": "Hubble Space Telescope", "expect": 1990, "image": "Hubble 2009 close-up.jpg", "tag": "spacecraft"},
                {"wiki": "International Space Station", "expect": 1998, "image": "The station pictured from the SpaceX Crew Dragon 5 (cropped).jpg", "tag": "spacecraft"},
                {"wiki": "James Webb Space Telescope", "expect": 2021, "image": "JWST spacecraft model 2.png", "tag": "spacecraft", "fit": "contain"},
            ],
        },
        {
            "id": "dinosaurs",
            "subject": "Dinosaur",
            "min_precision": 3,
            "events": [
                {"wiki": "Triassic", "expect": -251902000, "image": "Coelophysis (1).jpg", "tag": "fossil"},
                {"wiki": "Jurassic", "expect": -201400000, "image": "Stegosaurus ungulatus colored final.png", "tag": "animal", "fit": "contain"},
                {"wiki": "Cretaceous", "expect": -145000000, "image": "Tyrannosaur Rex and Triceratops.jpg", "tag": "fossil"},
                {"wiki": "Tyrannosaurus", "expect": -68000000, "image": "Tyrannosaurus Rex Holotype.jpg", "tag": "fossil"},
                {"wiki": "Cretaceous–Paleogene extinction event", "expect": -66000000, "image": "Impact event.jpg", "tag": "artwork"},
            ],
        },
        {
            "id": "roman_empire",
            "subject": "Roman Empire",
            "events": [
                {"wiki": "Assassination of Julius Caesar", "expect": -44, "image": "Retrato de Julio César (26724093101) (cropped).jpg", "tag": "portrait"},
                {"wiki": "Roman Empire", "expect": -27, "image": "Augustus of Prima Porta (inv. 2290).jpg", "tag": "portrait"},
                {"wiki": "Eruption of Mount Vesuvius in 79 AD", "expect": 79, "image": "Il cratere del Vulcano - panoramio.jpg", "tag": "nature"},
                {"wiki": "Hadrian's Wall", "expect": 122, "image": "Section of Hadrian's Wall 1.jpg", "tag": "building"},
                {"wiki": "Fall of the Western Roman Empire", "expect": 476, "image": "RomulusAugustus.jpg", "tag": "artifact", "fit": "contain", "author": "Classical Numismatic Group, Inc."},
            ],
        },
        {
            "id": "inventions",
            "subject": "Invention",
            "events": [
                {"wiki": "Telephone", "expect": 1876, "image": "Telefon BW 2012-02-18 13-44-32.JPG", "tag": "object", "fit": "contain"},
                {"wiki": "Incandescent light bulb", "expect": 1879, "image": "Gluehlampe 01 KMJ.jpg", "tag": "object", "fit": "contain"},
                {"wiki": "Benz Patent-Motorwagen", "expect": 1886, "image": "Patent-Motorwagen Nr.1 Benz 2.jpg", "tag": "vehicle"},
                {"wiki": "Wright Flyer", "expect": 1903, "image": "First flight2.jpg", "tag": "vehicle"},
                {"wiki": "World Wide Web", "expect": 1989, "image": "LS3 4919 (cropped).jpg", "tag": "portrait"},
            ],
        },
        {
            "id": "swiss_history",
            "subject": "History of Switzerland",
            "sensitive": True,
            "events": [
                {"wiki": "Federal Charter of 1291", "expect": 1291, "image": "Bundesbrief.jpg", "tag": "document"},
                {"wiki": "Battle of Morgarten", "expect": 1315, "image": "Morgarten Schlachtdenkmal 1.JPG", "tag": "building"},
                {"wiki": "Peace of Westphalia", "expect": 1648, "image": "Westfaelischer Friede in Muenster (Gerard Terborch 1648).jpg", "tag": "artwork"},
                {"wiki": "Swiss Federal Constitution", "expect": 1848, "image": "Bundesverfassung 1848.jpg", "tag": "document"},
                {"wiki": "Gotthard Tunnel", "expect": 1882, "image": "Goeschenen nordportal.jpg", "tag": "building"},
                {"wiki": "Gotthard Base Tunnel", "expect": 2016, "image": "20141120 gotthard-basistunnel02-wikipedia-hannes-ortlieb.jpg", "tag": "building"},
            ],
        },
    ],
}
