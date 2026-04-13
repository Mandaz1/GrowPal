/// <reference types="vite/client" />
import { useState, useEffect } from 'react';
import { collection, query, orderBy, getDocs } from 'firebase/firestore';
import { db } from './firebase';
import { XAxis, YAxis, Tooltip, ResponsiveContainer, CartesianGrid, ComposedChart, Bar, Line } from 'recharts';
import { Activity, Sun, Droplet, Trophy, Zap, Cpu } from 'lucide-react';
import { analyzeLogs, HealthLog, AiAnalysis } from './ai';

const RingGauge = ({ value, label, max = 100, isPercentage = false }: { value: number, label: string, max?: number, isPercentage?: boolean }) => {
  const radius = 60;
  const circumference = 2 * Math.PI * radius;
  const safeVal = Math.min(Math.max(Number(value) || 0, 0), max);
  const fill = (safeVal / max) * circumference;

  return (
    <div className="flex-1 flex flex-col items-center justify-center gap-6 w-full">
       <h3 className="text-[#39ff14] font-mono text-xl tracking-widest font-black uppercase text-center border-b-2 border-[#1f8a1f] pb-2 w-full opacity-90 drop-shadow-[0_0_8px_rgba(57,255,20,0.5)]">
          {label}
       </h3>
       
       <div className="relative w-40 h-40 drop-shadow-[0_0_10px_rgba(57,255,20,0.6)]">
          <svg viewBox="0 0 160 160" className="w-full h-full -rotate-90">
            <circle cx="80" cy="80" r={radius} fill="none" stroke="#0a2e0a" strokeWidth="16" />
            <circle cx="80" cy="80" r={radius} fill="none" stroke="#1f8a1f" strokeWidth="4" strokeDasharray="2 10" opacity="0.6" />
            <circle cx="80" cy="80" r={radius} fill="none" stroke="#39ff14" strokeWidth="16" strokeLinecap="round" strokeDasharray={`${fill} ${circumference}`} style={{ transition: 'stroke-dasharray 1s ease-out' }} />
          </svg>
          <div className="absolute inset-0 flex flex-col items-center justify-center mt-1">
             <span className="font-mono text-4xl text-[#39ff14] font-black italic">{value || 0}</span>
             {isPercentage && <span className="font-mono text-[#39ff14] text-xs opacity-70 tracking-widest uppercase mt-1">/ 100%</span>}
             {!isPercentage && <span className="font-mono text-[#39ff14] text-xs opacity-70 tracking-widest uppercase mt-1">/ Yield</span>}
          </div>
       </div>
    </div>
  );
}

const DiamondTrend = ({ trend }: { trend: 'UP' | 'DOWN' | 'STABLE' | null | undefined }) => {
  return (
    <div className="flex-1 flex flex-col items-center justify-center gap-6 w-full">
       <h3 className="text-[#39ff14] font-mono text-xl tracking-widest font-black uppercase text-center border-b-2 border-[#1f8a1f] pb-2 w-full opacity-90 drop-shadow-[0_0_8px_rgba(57,255,20,0.5)]">
          TREND_VR
       </h3>
       
       <div className="relative w-40 h-40 flex items-center justify-center drop-shadow-[0_0_10px_rgba(57,255,20,0.6)]">
          <svg viewBox="0 0 160 160" className="absolute inset-0 w-full h-full opacity-80">
            <polygon points="80,10 150,80 80,150 10,80" fill="none" stroke="#1f8a1f" strokeWidth="3" strokeDasharray="8 4" />
            <circle cx="80" cy="80" r="50" fill="none" stroke="#0a2e0a" strokeWidth="2" />
          </svg>
          
          <div className="z-10 flex flex-col items-center pt-2">
             {trend === 'UP' && <div className="text-5xl text-[#39ff14] mb-1 drop-shadow-[0_0_15px_#39ff14]">▲</div>}
             {trend === 'DOWN' && <div className="text-5xl text-[#39ff14] mt-1 drop-shadow-[0_0_15px_#39ff14]">▼</div>}
             {trend === 'STABLE' && <div className="text-5xl text-[#39ff14] drop-shadow-[0_0_15px_#39ff14]">■</div>}
             {!trend && <div className="text-5xl text-[#39ff14] opacity-50">-</div>}
             <span className="font-mono text-[#39ff14] text-lg font-black mt-1 tracking-widest italic">
                {trend === 'UP' ? 'ASCEND' : trend === 'DOWN' ? 'DESCEND' : trend === 'STABLE' ? 'STABLE' : 'NULL'}
             </span>
          </div>
       </div>
    </div>
  );
}

const CustomTooltip = ({ active, payload, label }: any) => {
  if (active && payload && payload.length) {
    return (
      <div className="bg-[#051005] border-2 border-[#39ff14] p-3 shadow-[0_0_15px_rgba(57,255,20,0.4)] rounded text-[#39ff14] font-mono uppercase">
        <p className="mb-2 opacity-80 border-b border-[#39ff14]/50 pb-1">{label}</p>
        <p className="font-black text-lg">MOTIVATION : {payload[0].value}</p>
      </div>
    );
  }
  return null;
};

const fallbackTrendData = [
  { day: 'No Data', motivation: 0 }
];

function MotherboardBackground() {
  return (
    <div className="fixed inset-0 pointer-events-none z-0 overflow-hidden opacity-50">
      <svg width="100%" height="100%" xmlns="http://www.w3.org/2000/svg" className="w-full h-full object-cover">
        
        {/* Massive Power Traces (Thick) */}
        <g stroke="var(--color-gold)" strokeWidth="16" strokeLinecap="square" strokeLinejoin="miter" fill="none" opacity="0.8">
          <path d="M 0,150 L 100,150 L 150,200 L 300,200 L 350,150 L 400,150 L 450,200" />
          <path d="M 1920,450 L 1700,450 L 1600,350 L 1400,350 L 1350,300 L 1200,300" />
          <path d="M 0,800 L 200,800 L 300,700 L 450,700 L 500,750" />
          <path d="M 1920,950 L 1500,950 L 1400,850 L 1200,850 L 1150,900 L 950,900" />
        </g>
        
        {/* Medium Traces with Vias */}
        <g stroke="var(--color-gold)" strokeWidth="8" strokeLinecap="square" strokeLinejoin="miter" fill="none" opacity="0.6">
          <path d="M 100,0 L 100,50 L 150,100 L 150,300 L 200,350 L 380,350" />
          <path d="M 500,0 L 500,100 L 450,150 L 450,400 L 500,450 L 580,450" />
          <path d="M 900,100 L 750,100 L 700,150 L 700,350 L 650,400 L 580,400" />
          <path d="M 1200,150 L 1000,150 L 950,200 L 950,500 L 900,550 L 780,550" />
          <path d="M 1500,100 L 1500,200 L 1400,300 L 1200,300 L 1150,350 L 1080,350" />
          <path d="M 1800,0 L 1800,150 L 1750,200 L 1750,600 L 1650,700 L 1580,700" />
          
          <path d="M 100,1080 L 100,900 L 150,850 L 300,850 L 350,800 L 480,800" />
          <path d="M 600,1080 L 600,1000 L 650,950 L 650,750 L 700,700 L 780,700" />
          <path d="M 1200,1080 L 1200,950 L 1150,900 L 950,900 L 900,850 L 780,850" />
          <path d="M 1700,1080 L 1700,850 L 1600,750 L 1450,750 L 1400,700 L 1280,700" />
        </g>
        
        {/* Fine Data Buses (Parallel Thin Lines) */}
        <g stroke="var(--color-gold)" strokeWidth="3" strokeLinecap="square" strokeLinejoin="miter" fill="none" opacity="0.4">
          <path d="M 200,0 L 200,180 L 250,230 L 250,400" />
          <path d="M 220,0 L 220,170 L 270,220 L 270,400" />
          <path d="M 240,0 L 240,160 L 290,210 L 290,400" />
          <path d="M 260,0 L 260,150 L 310,200 L 310,380" />
          
          <path d="M 0,400 L 80,400 L 130,450 L 130,550 L 180,600 L 260,600" />
          <path d="M 0,420 L 70,420 L 120,470 L 120,560 L 170,610 L 260,610" />
          <path d="M 0,440 L 60,440 L 110,490 L 110,570 L 160,620 L 260,620" />
          <path d="M 0,460 L 50,460 L 100,510 L 100,580 L 150,630 L 260,630" />
          
          <path d="M 1920,600 L 1800,600 L 1750,650 L 1750,800 L 1700,850 L 1550,850" />
          <path d="M 1920,620 L 1810,620 L 1760,670 L 1760,810 L 1710,860 L 1550,860" />
          <path d="M 1920,640 L 1820,640 L 1770,690 L 1770,820 L 1720,870 L 1550,870" />
          <path d="M 1920,660 L 1830,660 L 1780,710 L 1780,830 L 1730,880 L 1550,880" />
        </g>

        {/* Animated Phosphor Laser Pulses */}
        <g stroke="#39ff14" strokeWidth="6" strokeLinecap="round" strokeLinejoin="round" fill="none" opacity="1" className="drop-shadow-[0_0_12px_#39ff14]">
          {/* Pulses on Medium Traces */}
          <path className="trace-pulse" d="M 100,0 L 100,50 L 150,100 L 150,300 L 200,350 L 380,350" />
          <path className="trace-pulse-fast" d="M 500,0 L 500,100 L 450,150 L 450,400 L 500,450 L 580,450" />
          <path className="trace-pulse-slow" d="M 900,100 L 750,100 L 700,150 L 700,350 L 650,400 L 580,400" />
          <path className="trace-pulse" d="M 1200,150 L 1000,150 L 950,200 L 950,500 L 900,550 L 780,550" />
          <path className="trace-pulse-fast" d="M 1500,100 L 1500,200 L 1400,300 L 1200,300 L 1150,350 L 1080,350" />
          <path className="trace-pulse-slow" d="M 1800,0 L 1800,150 L 1750,200 L 1750,600 L 1650,700 L 1580,700" />
          
          <path className="trace-pulse" d="M 100,1080 L 100,900 L 150,850 L 300,850 L 350,800 L 480,800" />
          <path className="trace-pulse-fast" d="M 600,1080 L 600,1000 L 650,950 L 650,750 L 700,700 L 780,700" />
          <path className="trace-pulse-slow" d="M 1200,1080 L 1200,950 L 1150,900 L 950,900 L 900,850 L 780,850" />
          <path className="trace-pulse" d="M 1700,1080 L 1700,850 L 1600,750 L 1450,750 L 1400,700 L 1280,700" />
          
          {/* Pulses on Thick Power Traces */}
          <path className="trace-pulse-fast" strokeWidth="10" d="M 0,150 L 100,150 L 150,200 L 300,200 L 350,150 L 400,150 L 450,200" />
          <path className="trace-pulse" strokeWidth="10" d="M 1920,450 L 1700,450 L 1600,350 L 1400,350 L 1350,300 L 1200,300" />
          <path className="trace-pulse-slow" strokeWidth="10" d="M 0,800 L 200,800 L 300,700 L 450,700 L 500,750" />
          <path className="trace-pulse-fast" strokeWidth="10" d="M 1920,950 L 1500,950 L 1400,850 L 1200,850 L 1150,900 L 950,900" />
          
          {/* Pulses on Fine Data Buses */}
          <path className="trace-pulse-fast" strokeWidth="3" d="M 200,0 L 200,180 L 250,230 L 250,400" />
          <path className="trace-pulse" strokeWidth="3" d="M 240,0 L 240,160 L 290,210 L 290,400" />
          <path className="trace-pulse-slow" strokeWidth="3" d="M 0,400 L 80,400 L 130,450 L 130,550 L 180,600 L 260,600" />
          <path className="trace-pulse-fast" strokeWidth="3" d="M 0,440 L 60,440 L 110,490 L 110,570 L 160,620 L 260,620" />
          <path className="trace-pulse" strokeWidth="3" d="M 1920,600 L 1800,600 L 1750,650 L 1750,800 L 1700,850 L 1550,850" />
          <path className="trace-pulse-slow" strokeWidth="3" d="M 1920,640 L 1820,640 L 1770,690 L 1770,820 L 1720,870 L 1550,870" />
        </g>

        {/* Large Through-hole Copper Pads */}
        <g stroke="var(--color-gold)" fill="var(--color-pcb-dark)">
           <circle cx="450" cy="200" r="16" strokeWidth="6" />
           <circle cx="1200" cy="300" r="16" strokeWidth="6" />
           <circle cx="500" cy="750" r="16" strokeWidth="6" />
           <circle cx="950" cy="900" r="16" strokeWidth="6" />
           
           <circle cx="400" cy="350" r="10" strokeWidth="4" />
           <circle cx="600" cy="450" r="10" strokeWidth="4" />
           <circle cx="560" cy="400" r="10" strokeWidth="4" />
           <circle cx="760" cy="550" r="10" strokeWidth="4" />
           <circle cx="1060" cy="350" r="10" strokeWidth="4" />
           <circle cx="1560" cy="700" r="10" strokeWidth="4" />
           <circle cx="500" cy="800" r="10" strokeWidth="4" />
           <circle cx="800" cy="700" r="10" strokeWidth="4" />
           <circle cx="760" cy="850" r="10" strokeWidth="4" />
           <circle cx="1260" cy="700" r="10" strokeWidth="4" />
           
           <circle cx="330" cy="400" r="4" strokeWidth="2" />
           <circle cx="350" cy="400" r="4" strokeWidth="2" />
           <circle cx="370" cy="400" r="4" strokeWidth="2" />
           <circle cx="390" cy="400" r="4" strokeWidth="2" />

           <circle cx="280" cy="620" r="4" strokeWidth="2" />
           <circle cx="300" cy="620" r="4" strokeWidth="2" />
           <circle cx="320" cy="620" r="4" strokeWidth="2" />
           <circle cx="340" cy="620" r="4" strokeWidth="2" />

           <circle cx="1530" cy="850" r="4" strokeWidth="2" />
           <circle cx="1530" cy="870" r="4" strokeWidth="2" />
           <circle cx="1530" cy="890" r="4" strokeWidth="2" />
           <circle cx="1530" cy="910" r="4" strokeWidth="2" />
        </g>
      </svg>
    </div>
  );
}

function App() {
  const [viewMode, setViewMode] = useState<'daily' | 'allTime'>('daily');
  const [dailyData, setDailyData] = useState<any>({ steps: 0, sunlight: 0, water: 0, level: 0, missionsComplete: false });
  const [allTimeData, setAllTimeData] = useState<any>({ steps: 0, sunlight: 0, water: 0, level: 0, missionsComplete: false });
  const [aiAnalysis, setAiAnalysis] = useState<AiAnalysis | {error: string} | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    async function fetchData() {
      try {
        const deviceId = import.meta.env.VITE_DEVICE_ID || 'GrowPal_001';
        const logsRef = collection(db, 'users', deviceId, 'daily_logs');
        
        const q = query(logsRef, orderBy('__name__', 'desc')); 
        const querySnapshot = await getDocs(q);

        if (querySnapshot.empty) {
          setError('No data found for this device');
          setLoading(false);
          return;
        }

        let totalSteps = 0;
        let totalWater = 0;
        let totalSunlight = 0;

        querySnapshot.forEach((doc) => {
          const data = doc.data();
          totalSteps += (data.steps || 0);
          totalWater += (data.water || 0);
          totalSunlight += (data.sunlight_seconds || 0);
        });

        // The first document is the most recent because of 'desc' order
        const mostRecentData = querySnapshot.docs[0].data();

        setDailyData({
          steps: mostRecentData.steps || 0,
          sunlight: Math.floor((mostRecentData.sunlight_seconds || 0) / 60), 
          water: mostRecentData.water || 0,
          level: mostRecentData.level || 0,
          missionsComplete: mostRecentData.all_missions_done || false,
        });

        setAllTimeData({
          steps: totalSteps,
          sunlight: Math.floor(totalSunlight / 3600), 
          water: totalWater,
          level: mostRecentData.level || 0,
          missionsComplete: mostRecentData.all_missions_done || false,
        });

        // Parse logs for AI processing (take ALL logs chronologically, up to a reasonable limit like 100 to avoid huge payload, but enough for full visualization)
        const recentDocSnapshots = querySnapshot.docs.slice(0, 100).reverse();
        const logsForAi: HealthLog[] = recentDocSnapshots.map(doc => {
          const d = doc.data();
          // Assume document IDs are dates or sequential strings, else default
          return {
            dayId: doc.id,
            steps: d.steps || 0,
            water: d.water || 0,
            sunlight_seconds: d.sunlight_seconds || 0,
            missionsComplete: d.all_missions_done || false
          };
        });

        // Add LocalStorage caching to prevent Vite HMR from wiping out your API 15-RPM Quota on every save!
        const cacheKey = `ai_cache_${deviceId}_${querySnapshot.docs[0].id}`; 
        const cachedData = localStorage.getItem(cacheKey);

        if (cachedData) {
           console.log("Loading AI analytics from local cache...");
           setAiAnalysis(JSON.parse(cachedData));
        } else {
           console.log("Fetching new AI analytics from Gemini API...");
           const analysis = await analyzeLogs(logsForAi);
           
           // Only cache successful API responses, never errors
           if (analysis && !('error' in analysis)) {
              localStorage.setItem(cacheKey, JSON.stringify(analysis));
           }
           setAiAnalysis(analysis);
        }

      } catch (err: any) {
        console.error("Error fetching data:", err);
        setError(err.message);
      } finally {
        setLoading(false);
      }
    }

    fetchData();
  }, []);

  const data = viewMode === 'daily' ? dailyData : allTimeData;

  const cappedLevel = Math.min(Math.max(data.level, 0), 6);
  const currentLevelImage = `/Characters/lvl_${cappedLevel}.png`;

  if (loading) {
    return <div className="min-h-screen pcb-board flex items-center justify-center text-[var(--color-cyan)] font-bold text-2xl tracking-widest">\BOOTING_SYSTEM.../</div>;
  }

  if (error) {
    return <div className="min-h-screen pcb-board flex items-center justify-center text-red-500 font-bold text-2xl tracking-widest">X_SYSTEM_ERROR: {error}_X</div>;
  }

  // Use the fetched AI trend data, or fall back to an empty placeholder chart if API is missing
  const isAiError = aiAnalysis && 'error' in aiAnalysis;
  const aiData = aiAnalysis && !isAiError ? (aiAnalysis as AiAnalysis) : null;
  const aiErrorMessage = isAiError ? (aiAnalysis as {error: string}).error : null;

  const rawData = aiData && Array.isArray(aiData.trendData) ? aiData.trendData : fallbackTrendData;
  const chartData = rawData.map(d => ({
     day: String(d.day || 'Unknown'),
     motivation: Math.min(Math.max(Number(d.motivation) || 0, 0), 100)
  }));

  return (
    <div className="min-h-screen pcb-board flex flex-col items-center relative py-6">
      <MotherboardBackground />
      
      <div className="w-full max-w-6xl mx-auto p-4 md:p-8 relative z-10 flex flex-col flex-1">
        
        {/* Header HUD */}
        <header className="w-full flex justify-between items-center mb-10 pb-6 border-b-4 border-[var(--color-gold)] bg-[#e8e4d5]/90 px-6 rounded shadow-xl">
          <div className="flex items-center gap-4 mt-2">
            <Cpu className="text-[var(--color-text-main)] w-12 h-12" />
            <h1 className="text-3xl md:text-4xl uppercase tracking-widest font-black text-[var(--color-text-main)]">
              GrowPal_SYS
            </h1>
          </div>

          <div className="flex gap-2">
            <button onClick={() => setViewMode('daily')} className={`btn-pcb ${viewMode === 'daily' ? 'active' : ''}`}>
              DAILY
            </button>
            <button onClick={() => setViewMode('allTime')} className={`btn-pcb ${viewMode === 'allTime' ? 'active' : ''}`}>
              HISTORY
            </button>
          </div>
        </header>

        {/* 3 Column Data Arrangement */}
        <main className="w-full grid grid-cols-1 md:grid-cols-3 gap-8 mb-8 flex-1">
          
          <div className="flex flex-col gap-8 justify-center">
            <StatCard icon={<Activity className="text-[var(--color-orange)] w-8 h-8" />} label="STEPS" value={data.steps.toLocaleString()} unit="UNITS" />
            <StatCard icon={<Sun className="text-[var(--color-orange)] w-8 h-8" />} label="SOLAR I/O" value={data.sunlight.toString()} unit={viewMode === 'daily' ? 'MINS' : 'HRS'} />
          </div>

          <div className="motherboard-panel panel-pins flex flex-col items-center justify-center p-6 h-full min-h-[300px]">
            <div className="w-full flex justify-between px-2 text-[var(--color-text-main)] font-black uppercase text-sm mb-4">
               <span className="flex items-center gap-1">WIFI: LINKED</span>
               <span>BAT: 100%</span>
            </div>

            <div className="text-center mb-4 text-[var(--color-text-main)] text-2xl font-black tracking-widest border-b-2 border-[var(--color-text-main)] pb-2 w-3/4">
               LVL {data.level}
            </div>
            
            <div className="flex-1 w-full flex items-center justify-center relative motherboard-chip p-4 px-8 rounded">
               <img src={currentLevelImage} alt="Growpal Character" className="w-48 h-48 object-contain pixelated drop-shadow-[0_0_12px_rgba(255,255,255,0.4)]" />
               <div className="absolute inset-0 bg-[url('data:image/svg+xml,%3Csvg width=\'4\' height=\'4\' xmlns=\'http://www.w3.org/2000/svg\'%3E%3Cpath d=\'M0 0h4v1H0z\' fill=\'rgba(0,0,0,0.1)\'/%3E%3C/svg%3E')] pointer-events-none opacity-50 rounded"></div>
            </div>

            <div className="mt-6 flex gap-2 items-center bg-[var(--color-text-main)] text-[var(--color-surface)] px-6 py-2 rounded-full font-bold shadow-lg">
               <span className="text-sm uppercase tracking-widest">SYS_IDLE</span>
               <div className="w-3 h-3 rounded-full bg-[var(--color-green)] shadow-[0_0_5px_#558B2F] animate-pulse"></div>
            </div>
          </div>

          <div className="flex flex-col gap-8 justify-center">
            <StatCard icon={<Droplet className="text-[var(--color-cyan)] w-8 h-8" />} label="HYDRATION" value={data.water.toString()} unit="CUPS" />
            <div className="motherboard-panel panel-pins p-6 flex flex-col items-center justify-center relative h-full min-h-[140px] z-20">
               <Trophy className={`w-10 h-10 mb-4 ${data.missionsComplete ? 'text-[var(--color-green)]' : 'text-[#888]'}`} />
               <div className="text-sm text-[var(--color-text-main)] font-black uppercase tracking-widest mb-4 border-b-2 border-[#b5ac96] w-full text-center pb-2">
                 MISSION STATUS
               </div>
               <div className="w-full flex items-center justify-center motherboard-chip p-4">
                 {data.missionsComplete ? (
                   <span className="text-[var(--color-green)] font-black text-xl tracking-widest">CLEARED</span>
                 ) : (
                   <span className="text-[var(--color-orange)] font-black text-xl tracking-widest">PENDING</span>
                 )}
               </div>
            </div>
          </div>

        </main>

        {/* Dynamic AI Analytics Panel */}
        <section className="w-full motherboard-panel panel-pins p-6 mt-2 relative z-30 mb-8 flex flex-col">
          <h2 className="text-2xl font-black flex items-center gap-3 mb-6 border-b-2 border-[#b5ac96] pb-3 text-[var(--color-text-main)] uppercase tracking-widest">
             <Zap className="text-[var(--color-orange)] w-8 h-8" />
             AI DIAGNOSTIC TERMINAL
          </h2>
          
          <div className="grid grid-cols-1 lg:grid-cols-4 gap-8 flex-1">
            <div className="col-span-1 pr-6 flex flex-col justify-center border-r-2 border-[#b5ac96]">
              {aiData ? (
                <div className="flex flex-col gap-3">
                  <span className="text-[var(--color-orange)] text-lg mb-1 block border-b border-[#b5ac96] pb-1 font-bold">{"->"} SIGNAL ANALYSIS</span>
                  {aiData.analysis.map((msg, idx) => (
                    msg.type === 'WARN' ? (
                      <div key={idx} className="text-sm shadow-md leading-relaxed text-red-900 bg-red-500/20 p-3 rounded border-l-4 border-red-600 font-bold">
                        <span className="text-red-700 mr-2 uppercase tracking-widest">[WARN]</span>
                        {msg.message}
                      </div>
                    ) : (
                      <div key={idx} className="text-sm shadow-md leading-relaxed text-emerald-900 bg-emerald-500/20 p-3 rounded border-l-4 border-emerald-600 font-bold">
                        <span className="text-emerald-700 mr-2 uppercase tracking-widest">[OK]</span>
                        {msg.message}
                      </div>
                    )
                  ))}
                </div>
              ) : (
                <div className="text-sm leading-relaxed text-red-600 bg-red-100 p-4 rounded border-2 border-red-500 font-bold shadow-inner">
                  <span className="text-red-800 text-lg mb-2 block">{"!!"} API OFFLINE</span>
                  {aiErrorMessage ? aiErrorMessage : "Missing VITE_GEMINI_API_KEY inside the .env file. Add your API key to enable live analysis."}
                </div>
              )}
            </div>
            
            <div className="col-span-1 lg:col-span-3 h-full min-h-[400px] flex flex-col gap-6">
              
              {/* Phosphor CRT Graph */}
              <div className="w-full min-h-[300px] flex-1 bg-[#051005] border-8 border-[#3e2723] rounded-lg shadow-[inset_0_0_30px_rgba(0,0,0,1)] relative flex flex-col overflow-hidden">
                <ResponsiveContainer width="100%" height="100%" className="z-10 p-2 border-2 border-[#1a1a1a]">
                  <ComposedChart data={chartData} margin={{ top: 20, right: 30, left: -10, bottom: 0 }}>
                    <defs>
                      <linearGradient id="neonGreen" x1="0" y1="0" x2="0" y2="1">
                        <stop offset="5%" stopColor="#39ff14" stopOpacity={0.5}/>
                        <stop offset="95%" stopColor="#39ff14" stopOpacity={0}/>
                      </linearGradient>
                      <filter id="glow">
                        <feGaussianBlur stdDeviation="3" result="coloredBlur"/>
                        <feMerge>
                          <feMergeNode in="coloredBlur"/>
                          <feMergeNode in="SourceGraphic"/>
                        </feMerge>
                      </filter>
                    </defs>
                    <CartesianGrid stroke="#0a2e0a" strokeDasharray="2 2" vertical={true} />
                    <XAxis dataKey="day" stroke="#1f8a1f" tick={{fill: '#39ff14', fontSize: 13, fontFamily: 'monospace', fontWeight: 'bold'}} tickMargin={12} axisLine={{ strokeWidth: 2, stroke: '#1f8a1f' }} />
                    <YAxis domain={[0, 100]} ticks={[0, 20, 40, 60, 80, 100]} stroke="#1f8a1f" tick={{fill: '#39ff14', fontSize: 13, fontFamily: 'monospace', fontWeight: 'bold'}} axisLine={{ strokeWidth: 2, stroke: '#1f8a1f' }} />
                    <Tooltip 
                      content={<CustomTooltip />}
                      cursor={{fill: 'rgba(57, 255, 20, 0.1)'}}
                    />
                    <Bar dataKey="motivation" fill="url(#neonGreen)" barSize={40} />
                    <Line type="monotone" dataKey="motivation" stroke="#39ff14" strokeWidth={4} dot={{ r: 4, fill: '#fff', stroke: '#39ff14', strokeWidth: 2 }} activeDot={{ r: 8, fill: '#39ff14', stroke: '#fff', strokeWidth: 3 }} filter="url(#glow)" />
                  </ComposedChart>
                </ResponsiveContainer>
                
                {/* CRT Scanline Overlay */}
                <div className="absolute inset-0 pointer-events-none z-20 opacity-30 shadow-[inset_0_0_50px_rgba(0,0,0,1)] mix-blend-overlay flex flex-col">
                   {Array.from({length: 100}).map((_, i) => <div key={i} className="w-full h-[2px] bg-black mb-[2px]"></div>)}
                </div>
              </div>

              {/* HARDWARE METRICS PANEL (Unified CRT Cluster) */}
              <div className="w-full mt-4 flex flex-col bg-[#051005] border-8 border-[#3e2723] rounded-lg shadow-[inset_0_0_30px_rgba(0,0,0,1)] relative overflow-hidden p-8 z-10">
                 {/* CRT Scanline Overlay */}
                 <div className="absolute inset-0 pointer-events-none z-20 opacity-30 mix-blend-overlay flex flex-col">
                   {Array.from({length: 100}).map((_, i) => <div key={i} className="w-full h-[2px] bg-black mb-[2px]"></div>)}
                 </div>
                 
                 <div className="flex flex-col md:flex-row gap-12 lg:gap-24 items-center justify-between z-10 h-full w-full max-w-4xl mx-auto">
                    <RingGauge 
                      value={aiData ? aiData.overallMetrics.averageMotivation : 0} 
                      label="SYS_MOTIVATION" 
                      isPercentage={true}
                    />
                    <RingGauge 
                      value={aiData ? aiData.overallMetrics.engagementScore : 0} 
                      label="ENG_YIELD" 
                    />
                    <DiamondTrend trend={aiData ? aiData.overallMetrics.trendDirection : null} />
                 </div>
              </div>

            </div>
          </div>
        </section>

      </div>
    </div>
  );
}

// Reusable Stat Card Component
function StatCard({ icon, label, value, unit }: any) {
  return (
    <div className={`motherboard-panel panel-pins p-5 flex flex-col justify-between relative transition-transform duration-200 hover:-translate-y-1 h-full min-h-[140px] z-20`}>
      <div className="flex items-center gap-3 mb-2 border-b-2 border-[#b5ac96] pb-3 text-[var(--color-text-main)]">
        {icon}
        <span className="text-sm font-black uppercase tracking-widest">{label}</span>
      </div>
      
      <div className="flex items-center justify-between w-full motherboard-chip px-4 py-3 mt-auto rounded box-border shadow-inner">
        <span className="text-4xl font-black text-[var(--color-surface)] truncate leading-none mr-2">{value}</span>
        <span className="text-sm text-[#a8a8a8] font-bold uppercase tracking-widest flex-shrink-0">{unit}</span>
      </div>
    </div>
  );
}

export default App;
