// The Halo: Combat Evolved achievements (names, descriptions, points), in the list's order. Each one's
// number is the one the game writes to achievements.log when it is earned (port/linux/game/achievements.c),
// and its built-in picture is assets\icons\<number>.png (Build.bat). Everything starts locked.
namespace OpenCEAchievements
{
    static class Data
    {
        static Achievement A(int id, string name, string desc, int score)
        {
            return new Achievement(id, name, desc, score, 0L, new int[] { id });
        }

        public static readonly Achievement[] All = new Achievement[]
        {
            // Level completion
            A(1, "Pillar of Autumn", "Finish the first mission of the Campaign on any difficulty.", 25),
            A(2, "Halo", "Finish the second mission of the Campaign on any difficulty.", 25),
            A(3, "Truth and Reconciliation", "Finish the third mission of the Campaign on any difficulty.", 25),
            A(4, "The Silent Cartographer", "Finish the fourth mission of the Campaign on any difficulty.", 25),
            A(5, "Assault on the Control Room", "Finish the fifth mission of the Campaign on any difficulty.", 25),
            A(6, "343 Guilty Spark", "Finish the sixth mission of the Campaign on any difficulty.", 25),
            A(7, "The Library", "Finish the seventh mission of the Campaign on any difficulty.", 25),
            A(8, "Two Betrayals", "Finish the eighth mission of the Campaign on any difficulty.", 25),
            A(9, "Keyes", "Finish the ninth mission of the Campaign on any difficulty.", 25),
            A(10, "The Maw", "Finish the final mission of the Campaign on any difficulty.", 25),
            A(11, "Birth of a Spartan", "Finish the Campaign on Normal.", 10),
            A(12, "Believe in a Hero", "Finish the Campaign on Heroic (unlocks achievement for Normal, if not earned).", 20),
            A(13, "Living Legend", "Finish the Campaign on Legendary (unlocks achievement for Normal and Heroic, if not earned).", 50),
            A(14, "Standard Operating Brocedure", "Complete any level on Normal difficulty cooperatively.", 10),
            A(15, "Brovershield", "Complete any level on Heroic difficulty cooperatively.", 20),
            A(16, "Bro Hammer", "Complete any level on Legendary difficulty cooperatively.", 50),
            A(17, "He's Unstoppable!", "Complete any level on Heroic difficulty or higher without taking health damage.", 20),
            // Level-specific
            A(18, "Overshields are for Sissies", "Complete the level \"Pillar of Autumn\" on Legendary without picking up an Overshield.", 10),
            A(19, "Walk it Off", "Complete the level \"Pillar of Autumn\" on Legendary without picking up a health kit.", 25),
            A(20, "No-Fly Zone", "Destroy three Banshees in the level Halo on any difficulty.", 10),
            A(21, "How Pedestrian", "Complete the level \"Halo\" on any difficulty without entering a vehicle.", 25),
            A(22, "All According to Plan...", "Kill all the enemies in the first encounter of the level \"Truth and Reconciliation\" without being detected.", 10),
            A(23, "Close Quarters Combat", "Complete the level \"Truth and Reconciliation\" with at least four rounds left in your Sniper Rifle.", 25),
            A(24, "Beachhead", "Storm the Beach on the level \"The Silent Cartographer\" without losing any marines on Heroic difficulty or above.", 10),
            A(25, "Grenadier", "After activating the map in the level \"The Silent Cartographer\" complete the rest of your mission without firing a shot on Heroic difficulty or above.", 25),
            A(26, "Wraith Hunter", "Destroy four Wraith tanks in \"Assault on the Control Room\" with the Scorpion tank, in a single play.", 10),
            A(27, "I'll Be Taking That!", "Pilot a Banshee on the level \"Assault on the Control Room\".", 25),
            A(28, "This One's for Jenkins!", "Kill 50 Flood Combat Forms on the level \"343 Guilty Spark\" on Heroic difficulty or higher.", 10),
            A(29, "Breaking Quarantine", "Escape the Forerunner facility on the level \"343 Guilty Spark\" in 21 minutes.", 25),
            A(30, "That Just Happened", "Complete the level \"The Library\" on Heroic difficulty or higher without dying.", 10),
            A(31, "Speed Reader", "Complete the level \"The Library\" on Legendary difficulty in 30 minutes or less.", 25),
            A(32, "Look Out for the Little Guys", "Complete the level \"Two Betrayals\" on Heroic difficulty or higher without killing any Grunts.", 10),
            A(33, "Leave It Where It Lay", "Complete the level \"Two Betrayals\" on Legendary difficulty without picking up a new weapon.", 25),
            A(34, "Tying Up Loose Ends", "Kill every Elite on the level \"Keyes\" on Heroic difficulty or above.", 10),
            A(35, "Popcorn.gif", "Kill 100 Flood Infection Forms on the level \"Keyes\" on Heroic difficulty or above.", 25),
            A(36, "This Side Up", "Complete the Warthog ride on the level \"The Maw\" without being forcibly ejected from your vehicle.", 10),
            A(37, "Never Tell Me the Odds", "Complete the Warthog ride on the level \"The Maw\" on Legendary difficulty with at least a minute remaining on the countdown.", 25),
        };
    }
}
