        // Encapsulated namespace for the parallel segments visualization
        const ParallelSegments = (function() {
            const canvas = document.getElementById('canvas');
            const ctx = canvas.getContext('2d');

            // Color palette for segments
            const colors = [
                '#FF6B6B', '#4ECDC4', '#45B7D1', '#96CEB4', 
                '#FFEAA7', '#DDA0DD', '#98D8C8', '#F7DC6F',
                '#BB8FCE', '#85C1E9', '#F8C471', '#82E0AA'
            ];

            function getColorForSegment(name, index) {
                // Generate consistent color based on segment name
                let hash = 0;
                for (let i = 0; i < name.length; i++) {
                    hash = name.charCodeAt(i) + ((hash << 5) - hash);
                }
                return colors[Math.abs(hash) % colors.length];
            }

            // Helper function to convert coordinate to pixel position
            function coordToPixel(coord, range, lineStartX, lineWidth) {
                const rangeSize = range.end - range.start;
                const ratio = (coord - range.start) / rangeSize;
                return lineStartX + (ratio * lineWidth);
            }

            // Main drawing function - the only public interface
            function drawCoordinateBasedSegments(line1Data, line2Data, line1Y = 120, line2Y = 280, startX = 100, totalWidth = 700) {
                // Clear canvas
                ctx.clearRect(0, 0, canvas.width, canvas.height);
                
                const { range: line1Range, segments: line1Segments, label: line1Label } = line1Data;
                const { range: line2Range, segments: line2Segments, label: line2Label } = line2Data;

                // Collect all unique segment names from both lines
                const allSegmentNames = new Set([
                    ...line1Segments.map(s => s.name),
                    ...line2Segments.map(s => s.name)
                ]);

                // Draw polygons connecting corresponding segments by name
                allSegmentNames.forEach(segmentName => {
                    const segment1 = line1Segments.find(seg => seg.name === segmentName);
                    const segment2 = line2Segments.find(seg => seg.name === segmentName);
                    
                    // Skip if segment doesn't exist on both lines
                    if (!segment1 || !segment2) return;

                    const color = getColorForSegment(segmentName, 0);

                    // Calculate pixel positions for line 1 segment
                    const x1_start = coordToPixel(segment1.start, line1Range, startX, totalWidth);
                    const x1_end = coordToPixel(segment1.end, line1Range, startX, totalWidth);

                    // Calculate pixel positions for line 2 segment
                    const x2_start = coordToPixel(segment2.start, line2Range, startX, totalWidth);
                    const x2_end = coordToPixel(segment2.end, line2Range, startX, totalWidth);

                    // Check if segment is reversed (start > end on either line)
                    const line1Reversed = segment1.start > segment1.end;
                    const line2Reversed = segment2.start > segment2.end;
                    const isReversed = line1Reversed !== line2Reversed;

                    // Draw polygon between the segments
                    ctx.fillStyle = color;
                    ctx.globalAlpha = 0.6;
                    ctx.beginPath();
                    
                    if (isReversed) {
                        // Draw smooth hourglass-shaped polygon for reversed segments with more control points
                        const midY = (line1Y + line2Y) / 2;
                        const waistWidth = Math.abs(x1_end - x1_start) * 0.2; // Narrower waist for more dramatic effect
                        const waistCenterX = (x1_start + x1_end + x2_start + x2_end) / 4;
                        const waistLeft = waistCenterX - waistWidth / 2;
                        const waistRight = waistCenterX + waistWidth / 2;
                        
                        // Start at top-left
                        ctx.moveTo(x1_start, line1Y);
                        
                        // Top edge (straight line)
                        ctx.lineTo(x1_end, line1Y);
                        
                        // Right side curve from top to waist (multiple segments for smoothness)
                        // First quarter curve
                        const q1Y = line1Y + (midY - line1Y) * 0.25;
                        const q1X = x1_end + (waistRight - x1_end) * 0.1;
                        ctx.quadraticCurveTo(x1_end + (waistRight - x1_end) * 0.05, line1Y + (midY - line1Y) * 0.1, q1X, q1Y);
                        
                        // Second quarter curve (more aggressive inward curve)
                        const q2Y = line1Y + (midY - line1Y) * 0.5;
                        const q2X = x1_end + (waistRight - x1_end) * 0.4;
                        ctx.quadraticCurveTo(x1_end + (waistRight - x1_end) * 0.2, line1Y + (midY - line1Y) * 0.3, q2X, q2Y);
                        
                        // Third quarter curve (approaching waist)
                        const q3Y = line1Y + (midY - line1Y) * 0.75;
                        const q3X = x1_end + (waistRight - x1_end) * 0.7;
                        ctx.quadraticCurveTo(x1_end + (waistRight - x1_end) * 0.55, line1Y + (midY - line1Y) * 0.6, q3X, q3Y);
                        
                        // Final curve to waist
                        ctx.quadraticCurveTo(x1_end + (waistRight - x1_end) * 0.85, line1Y + (midY - line1Y) * 0.9, waistRight, midY);
                        
                        // Right side curve from waist to bottom (mirror of top)
                        // First quarter from waist
                        const b1Y = midY + (line2Y - midY) * 0.25;
                        const b1X = waistRight + (x2_end - waistRight) * 0.3;
                        ctx.quadraticCurveTo(waistRight + (x2_end - waistRight) * 0.15, midY + (line2Y - midY) * 0.1, b1X, b1Y);
                        
                        // Second quarter
                        const b2Y = midY + (line2Y - midY) * 0.5;
                        const b2X = waistRight + (x2_end - waistRight) * 0.6;
                        ctx.quadraticCurveTo(waistRight + (x2_end - waistRight) * 0.45, midY + (line2Y - midY) * 0.35, b2X, b2Y);
                        
                        // Third quarter
                        const b3Y = midY + (line2Y - midY) * 0.75;
                        const b3X = waistRight + (x2_end - waistRight) * 0.8;
                        ctx.quadraticCurveTo(waistRight + (x2_end - waistRight) * 0.7, midY + (line2Y - midY) * 0.6, b3X, b3Y);
                        
                        // Final curve to bottom-right
                        ctx.quadraticCurveTo(waistRight + (x2_end - waistRight) * 0.9, midY + (line2Y - midY) * 0.9, x2_end, line2Y);
                        
                        // Bottom edge (straight line)
                        ctx.lineTo(x2_start, line2Y);
                        
                        // Left side curve from bottom to waist (mirror of right side)
                        // First quarter from bottom
                        const bl1Y = line2Y - (line2Y - midY) * 0.25;
                        const bl1X = x2_start - (x2_start - waistLeft) * 0.3;
                        ctx.quadraticCurveTo(x2_start - (x2_start - waistLeft) * 0.15, line2Y - (line2Y - midY) * 0.1, bl1X, bl1Y);
                        
                        // Second quarter
                        const bl2Y = line2Y - (line2Y - midY) * 0.5;
                        const bl2X = x2_start - (x2_start - waistLeft) * 0.6;
                        ctx.quadraticCurveTo(x2_start - (x2_start - waistLeft) * 0.45, line2Y - (line2Y - midY) * 0.35, bl2X, bl2Y);
                        
                        // Third quarter
                        const bl3Y = line2Y - (line2Y - midY) * 0.75;
                        const bl3X = x2_start - (x2_start - waistLeft) * 0.8;
                        ctx.quadraticCurveTo(x2_start - (x2_start - waistLeft) * 0.7, line2Y - (line2Y - midY) * 0.6, bl3X, bl3Y);
                        
                        // Final curve to waist
                        ctx.quadraticCurveTo(x2_start - (x2_start - waistLeft) * 0.9, line2Y - (line2Y - midY) * 0.9, waistLeft, midY);
                        
                        // Left side curve from waist to top (mirror of bottom to waist)
                        // First quarter from waist
                        const tl1Y = midY - (midY - line1Y) * 0.25;
                        const tl1X = waistLeft - (waistLeft - x1_start) * 0.3;
                        ctx.quadraticCurveTo(waistLeft - (waistLeft - x1_start) * 0.15, midY - (midY - line1Y) * 0.1, tl1X, tl1Y);
                        
                        // Second quarter
                        const tl2Y = midY - (midY - line1Y) * 0.5;
                        const tl2X = waistLeft - (waistLeft - x1_start) * 0.6;
                        ctx.quadraticCurveTo(waistLeft - (waistLeft - x1_start) * 0.45, midY - (midY - line1Y) * 0.35, tl2X, tl2Y);
                        
                        // Third quarter
                        const tl3Y = midY - (midY - line1Y) * 0.75;
                        const tl3X = waistLeft - (waistLeft - x1_start) * 0.8;
                        ctx.quadraticCurveTo(waistLeft - (waistLeft - x1_start) * 0.7, midY - (midY - line1Y) * 0.6, tl3X, tl3Y);
                        
                        // Final curve to top-left
                        ctx.quadraticCurveTo(waistLeft - (waistLeft - x1_start) * 0.9, midY - (midY - line1Y) * 0.9, x1_start, line1Y);
                    } else {
                        // Draw normal polygon with straight lines
                        ctx.moveTo(x1_start, line1Y);
                        ctx.lineTo(x1_end, line1Y);
                        ctx.lineTo(x2_end, line2Y);
                        ctx.lineTo(x2_start, line2Y);
                    }
                    
                    ctx.closePath();
                    ctx.fill();

                    // Draw polygon border with different style for reversed segments
                    ctx.globalAlpha = 1;
                    ctx.strokeStyle = isReversed ? '#FF0000' : color;
                    ctx.lineWidth = isReversed ? 2 : 1;
                    ctx.setLineDash(isReversed ? [5, 5] : []);
                    
                    // Redraw the border path (EXACT same path as fill)
                    ctx.beginPath();
                    if (isReversed) {
                        // Redraw exact hourglass border with all control points
                        const midY = (line1Y + line2Y) / 2;
                        const waistWidth = Math.abs(x1_end - x1_start) * 0.2;
                        const waistCenterX = (x1_start + x1_end + x2_start + x2_end) / 4;
                        const waistLeft = waistCenterX - waistWidth / 2;
                        const waistRight = waistCenterX + waistWidth / 2;
                        
                        // Start at top-left
                        ctx.moveTo(x1_start, line1Y);
                        
                        // Top edge (straight line)
                        ctx.lineTo(x1_end, line1Y);
                        
                        // Right side curve from top to waist (exact same as fill)
                        const q1Y = line1Y + (midY - line1Y) * 0.25;
                        const q1X = x1_end + (waistRight - x1_end) * 0.1;
                        ctx.quadraticCurveTo(x1_end + (waistRight - x1_end) * 0.05, line1Y + (midY - line1Y) * 0.1, q1X, q1Y);
                        
                        const q2Y = line1Y + (midY - line1Y) * 0.5;
                        const q2X = x1_end + (waistRight - x1_end) * 0.4;
                        ctx.quadraticCurveTo(x1_end + (waistRight - x1_end) * 0.2, line1Y + (midY - line1Y) * 0.3, q2X, q2Y);
                        
                        const q3Y = line1Y + (midY - line1Y) * 0.75;
                        const q3X = x1_end + (waistRight - x1_end) * 0.7;
                        ctx.quadraticCurveTo(x1_end + (waistRight - x1_end) * 0.55, line1Y + (midY - line1Y) * 0.6, q3X, q3Y);
                        
                        ctx.quadraticCurveTo(x1_end + (waistRight - x1_end) * 0.85, line1Y + (midY - line1Y) * 0.9, waistRight, midY);
                        
                        // Right side curve from waist to bottom
                        const b1Y = midY + (line2Y - midY) * 0.25;
                        const b1X = waistRight + (x2_end - waistRight) * 0.3;
                        ctx.quadraticCurveTo(waistRight + (x2_end - waistRight) * 0.15, midY + (line2Y - midY) * 0.1, b1X, b1Y);
                        
                        const b2Y = midY + (line2Y - midY) * 0.5;
                        const b2X = waistRight + (x2_end - waistRight) * 0.6;
                        ctx.quadraticCurveTo(waistRight + (x2_end - waistRight) * 0.45, midY + (line2Y - midY) * 0.35, b2X, b2Y);
                        
                        const b3Y = midY + (line2Y - midY) * 0.75;
                        const b3X = waistRight + (x2_end - waistRight) * 0.8;
                        ctx.quadraticCurveTo(waistRight + (x2_end - waistRight) * 0.7, midY + (line2Y - midY) * 0.6, b3X, b3Y);
                        
                        ctx.quadraticCurveTo(waistRight + (x2_end - waistRight) * 0.9, midY + (line2Y - midY) * 0.9, x2_end, line2Y);
                        
                        // Bottom edge (straight line)
                        ctx.lineTo(x2_start, line2Y);
                        
                        // Left side curve from bottom to waist
                        const bl1Y = line2Y - (line2Y - midY) * 0.25;
                        const bl1X = x2_start - (x2_start - waistLeft) * 0.3;
                        ctx.quadraticCurveTo(x2_start - (x2_start - waistLeft) * 0.15, line2Y - (line2Y - midY) * 0.1, bl1X, bl1Y);
                        
                        const bl2Y = line2Y - (line2Y - midY) * 0.5;
                        const bl2X = x2_start - (x2_start - waistLeft) * 0.6;
                        ctx.quadraticCurveTo(x2_start - (x2_start - waistLeft) * 0.45, line2Y - (line2Y - midY) * 0.35, bl2X, bl2Y);
                        
                        const bl3Y = line2Y - (line2Y - midY) * 0.75;
                        const bl3X = x2_start - (x2_start - waistLeft) * 0.8;
                        ctx.quadraticCurveTo(x2_start - (x2_start - waistLeft) * 0.7, line2Y - (line2Y - midY) * 0.6, bl3X, bl3Y);
                        
                        ctx.quadraticCurveTo(x2_start - (x2_start - waistLeft) * 0.9, line2Y - (line2Y - midY) * 0.9, waistLeft, midY);
                        
                        // Left side curve from waist to top
                        const tl1Y = midY - (midY - line1Y) * 0.25;
                        const tl1X = waistLeft - (waistLeft - x1_start) * 0.3;
                        ctx.quadraticCurveTo(waistLeft - (waistLeft - x1_start) * 0.15, midY - (midY - line1Y) * 0.1, tl1X, tl1Y);
                        
                        const tl2Y = midY - (midY - line1Y) * 0.5;
                        const tl2X = waistLeft - (waistLeft - x1_start) * 0.6;
                        ctx.quadraticCurveTo(waistLeft - (waistLeft - x1_start) * 0.45, midY - (midY - line1Y) * 0.35, tl2X, tl2Y);
                        
                        const tl3Y = midY - (midY - line1Y) * 0.75;
                        const tl3X = waistLeft - (waistLeft - x1_start) * 0.8;
                        ctx.quadraticCurveTo(waistLeft - (waistLeft - x1_start) * 0.7, midY - (midY - line1Y) * 0.6, tl3X, tl3Y);
                        
                        ctx.quadraticCurveTo(waistLeft - (waistLeft - x1_start) * 0.9, midY - (midY - line1Y) * 0.9, x1_start, line1Y);
                    } else {
                        // Redraw normal polygon border
                        ctx.moveTo(x1_start, line1Y);
                        ctx.lineTo(x1_end, line1Y);
                        ctx.lineTo(x2_end, line2Y);
                        ctx.lineTo(x2_start, line2Y);
                    }
                    ctx.closePath();
                    ctx.stroke();
                    ctx.setLineDash([]); // Reset dash

                    // Add small indicator for reversed segments
                    if (isReversed) {
                        ctx.fillStyle = '#FF0000';
                        ctx.font = '10px Arial';
                        ctx.textAlign = 'center';
                        ctx.fillText('↻', (x1_start + x1_end) / 2, (line1Y + line2Y) / 2);
                    }
                });

                // Draw the parallel lines
                ctx.strokeStyle = '#333';
                ctx.lineWidth = 3;
                
                // Top line
                ctx.beginPath();
                ctx.moveTo(startX, line1Y);
                ctx.lineTo(startX + totalWidth, line1Y);
                ctx.stroke();

                // Bottom line
                ctx.beginPath();
                ctx.moveTo(startX, line2Y);
                ctx.lineTo(startX + totalWidth, line2Y);
                ctx.stroke();

                // Draw regularly spaced tick marks and labels
                ctx.lineWidth = 1;
                ctx.font = '10px Arial';
                ctx.fillStyle = '#666';

                // Helper function to format numbers with commas
                function formatWithCommas(num) {
                    return num.toLocaleString();
                }

                // Line 1 regular ticks (integers only)
                const line1Range_span = line1Range.end - line1Range.start;
                const line1TickCount = Math.min(8, Math.floor(line1Range_span));
                const line1Step = Math.max(1, Math.floor(line1Range_span / line1TickCount));
                
                for (let coord = Math.ceil(line1Range.start); coord <= line1Range.end; coord += line1Step) {
                    if (coord > line1Range.end) break;
                    const x = coordToPixel(coord, line1Range, startX, totalWidth);
                    
                    // Draw tick mark
                    ctx.beginPath();
                    ctx.moveTo(x, line1Y - 6);
                    ctx.lineTo(x, line1Y + 6);
                    ctx.stroke();
                    
                    // Draw coordinate label above the line with commas
                    ctx.textAlign = 'center';
                    ctx.fillText(formatWithCommas(coord), x, line1Y - 12);
                }

                // Line 2 regular ticks (integers only)
                const line2Range_span = line2Range.end - line2Range.start;
                const line2TickCount = Math.min(8, Math.floor(line2Range_span));
                const line2Step = Math.max(1, Math.floor(line2Range_span / line2TickCount));
                
                for (let coord = Math.ceil(line2Range.start); coord <= line2Range.end; coord += line2Step) {
                    if (coord > line2Range.end) break;
                    const x = coordToPixel(coord, line2Range, startX, totalWidth);
                    
                    // Draw tick mark
                    ctx.beginPath();
                    ctx.moveTo(x, line2Y - 6);
                    ctx.lineTo(x, line2Y + 6);
                    ctx.stroke();
                    
                    // Draw coordinate label below the line with commas
                    ctx.textAlign = 'center';
                    ctx.fillText(formatWithCommas(coord), x, line2Y + 18);
                }

                // Add custom line labels
                ctx.fillStyle = '#333';
                ctx.font = '14px Arial';
                ctx.textAlign = 'left';
                
                // Format line labels as "chr1 start-end (extent bp)"
                const line1Start = line1Range.start.toLocaleString();
                const line1End = line1Range.end.toLocaleString();
                const line1Extent = (line1Range.end - line1Range.start).toLocaleString();
                
                const line2Start = line2Range.start.toLocaleString();
                const line2End = line2Range.end.toLocaleString();
                const line2Extent = (line2Range.end - line2Range.start).toLocaleString();
                
                ctx.fillText(`chr1: ${line1Start}-${line1End} (${line1Extent} bp)`, startX - 90, line1Y - 60);
                ctx.fillText(`chr1: ${line2Start}-${line2End} (${line2Extent} bp)`, startX - 90, line2Y + 85);
            }

            // Return only the public interface
            return {
                drawCoordinateBasedSegments: drawCoordinateBasedSegments
            };
        })();

        // Example functions that use the public interface (outside the namespace)
        function drawExample1() {
            const line1Data = {
                range: { start: 0, end: 24 }, // 24-hour format
                label: "Monday Schedule",
                segments: [
                    { name: 'Meeting', start: 2, end: 4 },   // 2 hours each
                    { name: 'Work', start: 6, end: 8 },      // 2 hours each
                    { name: 'Break', start: 12, end: 14 },   // 2 hours each
                    { name: 'Review', start: 18, end: 20 }   // 2 hours each
                ]
            };

            const line2Data = {
                range: { start: 0, end: 24 }, // Same scale
                label: "Tuesday Schedule",
                segments: [
                    { name: 'Meeting', start: 10, end: 8 },  // 2 hours, REVERSED!
                    { name: 'Work', start: 14, end: 16 },    // 2 hours each
                    { name: 'Break', start: 6, end: 4 },     // 2 hours, REVERSED!
                    { name: 'Review', start: 20, end: 22 }   // 2 hours each
                ]
            };

            ParallelSegments.drawCoordinateBasedSegments(line1Data, line2Data);
        }

        function drawExample2() {
            const line1Data = {
                range: { start: 0, end: 100 }, // Percentage scale (0-100%)
                label: "Forward Process",
                segments: [
                    { name: 'Init', start: 10, end: 25 },      // 15% each
                    { name: 'Process', start: 30, end: 45 },   // 15% each
                    { name: 'Validate', start: 55, end: 70 },  // 15% each
                    { name: 'Complete', start: 80, end: 95 }   // 15% each
                ]
            };

            const line2Data = {
                range: { start: 0, end: 200 }, // Different scale
                label: "Reverse Process",
                segments: [
                    { name: 'Init', start: 70, end: 40 },      // 30 units, REVERSED!
                    { name: 'Process', start: 130, end: 100 }, // 30 units, REVERSED!
                    { name: 'Validate', start: 40, end: 10 },  // 30 units, REVERSED!
                    { name: 'Complete', start: 190, end: 160 } // 30 units, REVERSED!
                ]
            };

            ParallelSegments.drawCoordinateBasedSegments(line2Data, line1Data); // Swapped to show different pattern
        }

        function drawCustom() {
            const line1Data = {
                range: { start: 100000, end: 150000 }, // 5-digit range extent (50,000)
                label: "Production Timeline",
                segments: [
                    { name: 'InitPhase', start: 105000, end: 115000 },  // 10k units each, normal
                    { name: 'BuildPhase', start: 120000, end: 130000 }, // 10k units each, normal
                    { name: 'TestPhase', start: 135000, end: 145000 },  // 10k units each, normal
                    { name: 'LaunchPhase', start: 146000, end: 148000 } // 2k units, normal
                ]
            };

            const line2Data = {
                range: { start: 200000, end: 275000 }, // 5-digit range extent (75,000)
                label: "Resource Allocation",
                segments: [
                    { name: 'InitPhase', start: 220000, end: 205000 },   // 15k units, REVERSED!
                    { name: 'BuildPhase', start: 235000, end: 250000 },  // 15k units, normal
                    { name: 'TestPhase', start: 270000, end: 255000 },   // 15k units, REVERSED!
                    { name: 'LaunchPhase', start: 272000, end: 274000 }  // 2k units, normal
                ]
            };

            ParallelSegments.drawCoordinateBasedSegments(line1Data, line2Data);
        }

