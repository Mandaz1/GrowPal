import { GoogleGenAI } from "@google/genai";

let ai: any = null;
try {
  const key = import.meta.env.VITE_GEMINI_API_KEY;
  if (key && key !== "undefined" && key.length > 10) {
    ai = new GoogleGenAI({ apiKey: key });
  } else {
    console.warn("Gemini API Key missing or invalid. Please set VITE_GEMINI_API_KEY.");
  }
} catch (e) {
  console.warn("Gemini API Key initialization failed:", e);
}

export type HealthLog = {
  dayId: string;
  steps: number;
  water: number;
  sunlight_seconds: number;
  missionsComplete: boolean;
};

export type AnalysisMessage = {
  type: "OK" | "WARN";
  message: string;
};

export type AiAnalysis = {
  trendData: { day: string; motivation: number }[];
  analysis: AnalysisMessage[];
  overallMetrics: {
    averageMotivation: number;
    engagementScore: number;
    trendDirection: "UP" | "DOWN" | "STABLE";
  };
};

export async function analyzeLogs(logs: HealthLog[]): Promise<AiAnalysis | {error: string}> {
  if (!ai) {
    return { error: "No Gemini API key found. Please add VITE_GEMINI_API_KEY to your .env file." };
  }
  
  const prompt = `You are the GrowPal AI, an encouraging and easy-to-understand health assistant.
The user has provided an array of their health logs.
Step 1: Calculate "motivation" (0-100) for EVERY single day provided in the logs and create the "trendData" array. It is critical that the trendData array has the exact same number of items as the Logs Data provided.
Step 2: Calculate overall performance metrics based on all data. Provide an "averageMotivation" score (0-100), an "engagementScore" (0-100 based on mission completion and general tracking frequency), and the "trendDirection" (UP, DOWN, or STABLE).
Step 3: Provide 2 short, highly understandable feedback points based ONLY on the most recent 7 days of data (the last 7 items in the array). 
- Output them as an array of objects under "analysis", where "type" is either "OK" or "WARN".
- Make the language conversational, motivating, and simple to understand for a normal person.

Logs Data:
${JSON.stringify(logs, null, 2)}

You MUST respond exclusively with valid JSON matching exactly this structure:
{
  "trendData": [{ "day": "Day 1", "motivation": 85 }],
  "overallMetrics": {
    "averageMotivation": 82,
    "engagementScore": 90,
    "trendDirection": "UP"
  },
  "analysis": [
    { "type": "OK", "message": "You drank plenty of water this week, great job!" },
    { "type": "WARN", "message": "Your step count is dropping. Try to take a walk tomorrow!" }
  ]
}
`;

  try {
    const response = await ai.models.generateContent({
      model: "gemini-3-flash-preview",
      contents: prompt,
      config: {
        responseMimeType: "application/json"
      }
    });

    const text = response.text;
    const cleanedText = text.replace(/```json/g, '').replace(/```/g, '').trim();
    return JSON.parse(cleanedText) as AiAnalysis;
  } catch (err: any) {
    console.error("AI Generation Error details: ", err?.message || err);
    return { error: err?.message || String(err) };
  }
}

