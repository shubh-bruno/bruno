import StyledWrapper from './StyledWrapper';
import Docs from '../Docs';
import Info from './Info';
import { IconBox } from '@tabler/icons';
import RequestsNotLoaded from './RequestsNotLoaded';
import MountTime from './MountTime';

const Overview = ({ collection }) => {
  return (
    <div className="h-full overflow-hidden">
      <div className="grid grid-cols-5 gap-5 h-full">
        <div className="col-span-2 overflow-y-auto overflow-x-clip text-ellipsis pr-2">
          <div className="flex gap-2 items-center min-w-0">
            <IconBox size={20} stroke={1.5} className="flex-shrink-0" />
            <span className="overflow-hidden text-lg font-medium whitespace-nowrap text-ellipsis">
              {collection?.name}
            </span>
          </div>
          <Info collection={collection} />
          <RequestsNotLoaded collection={collection} />
        </div>
        <div className="col-span-3 overflow-hidden flex flex-col">
          <div className="flex-1 min-h-0">
            <Docs collection={collection} />
          </div>
          <MountTime collection={collection} />
        </div>
      </div>
    </div>
  );
};

export default Overview;
