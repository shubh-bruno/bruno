import { IconClock } from '@tabler/icons';
import StyledWrapper from './StyledWrapper';

const formatDuration = (ms) => (ms < 1000 ? `${Math.round(ms)} ms` : `${(ms / 1000).toFixed(2)} s`);

const MountTime = ({ collection }) => {
  const { mountDurationMs, mountStatus, isLoading } = collection;

  let value = '-';
  if (mountDurationMs != null) {
    value = formatDuration(mountDurationMs);
  } else if (mountStatus === 'mounting' || isLoading) {
    value = 'Mounting...';
  }

  return (
    <StyledWrapper className="flex items-center gap-2 pt-4" data-testid="collection-mount-time">
      <IconClock size={20} strokeWidth={1.5} />
      <span className="text-lg font-medium">Mount time</span>
      <span className="mount-time-value" data-testid="collection-mount-time-value">{value}</span>
    </StyledWrapper>
  );
};

export default MountTime;
